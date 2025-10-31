module;

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module smhkd.key_handler;

import smhkd.hotkey;
import smhkd.chord;
import smhkd.modifier;
import smhkd.log;
import smhkd.utils;
import smhkd.service;
import smhkd.parser;
import smhkd.interpreter;
import smhkd.ast;
import smhkd.log;

export namespace smhkd::key_handler {
struct KeyHandler {
    std::string configFileName;

    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    std::map<Hotkey, std::string> hotkeys;
    ConfigProperties config;

    std::optional<Chord> lastTriggeredChord;
    std::vector<Chord> currentChords;
    std::chrono::time_point<std::chrono::system_clock> lastKeyPressTime;

    explicit KeyHandler(const std::string& configFileName) : configFileName(configFileName) {
        loadConfig(configFileName);
    }

    bool init();
    void run() const;
    void loadConfig(const std::string& config_file);
    bool setupEventTap();
    [[nodiscard]] static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon);
    [[nodiscard]] bool handleKeyEvent(CGEventRef event, CGEventType type);

    void reload() {
        loadConfig(configFileName);
    }

   private:
    void clearSequence();
    bool checkAndExecuteSequence(const Chord& current);
};

inline bool KeyHandler::init() {
    runLoop = CFRunLoopGetCurrent();
    if (!runLoop) return false;
    info("run loop initialized");
    if (!setupEventTap()) return false;
    debug("event tap initialized");
    return true;
}

inline bool KeyHandler::setupEventTap() {
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);
    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, eventCallback, this);
    if (!tap) error("failed to create event tap");
    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);
    eventTap = tap;
    CFRelease(runLoopSource);
    return true;
}

inline CGEventRef KeyHandler::eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
    auto* keyHandler = static_cast<KeyHandler*>(refcon);
    if (keyHandler->handleKeyEvent(event, type)) return nullptr;
    return event;
}

inline void KeyHandler::clearSequence() {
    currentChords.clear();
    lastKeyPressTime = std::chrono::time_point<std::chrono::system_clock>::min();
}

inline bool KeyHandler::checkAndExecuteSequence(const Chord& current) {
    auto now = std::chrono::system_clock::now();
    if (now != std::chrono::time_point<std::chrono::system_clock>::min() && now - lastKeyPressTime > config.maxChordInterval) {
        clearSequence();
        return false;
    }
    lastKeyPressTime = now;
    currentChords.push_back(current);

    for (const auto& [hotkey, command] : hotkeys) {
        if (hotkey.chords.size() == 1) continue;
        if (currentChords.size() > hotkey.chords.size()) continue;

        bool matches = true;
        for (size_t i = 0; i < currentChords.size(); i++) {
            if (!(hotkey.chords[i].isActivatedBy(currentChords[i]))) {
                matches = false;
                break;
            }
        }
        if (!matches) continue;

        if (currentChords.size() == hotkey.chords.size()) {
            debug("Matched complete chord sequence");
            if (!command.empty()) {
                debug("executing command: {}", command);
                executeCommand(command);
            }
            clearSequence();
            return true;
        }
        debug("Matched partial chord sequence");
        return true;
    }
    clearSequence();
    return false;
}

inline bool KeyHandler::handleKeyEvent(CGEventRef event, CGEventType type) {
    auto keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    CGEventFlags flags = CGEventGetFlags(event);
    bool isRepeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

    Chord current{
        .keysym = {.keycode = keyCode},
        .modifiers = eventModifierFlagsToHotkeyFlags(flags),
    };

    auto exitChord = Chord{
        .keysym = {.keycode = 8},
        .modifiers = {.flags = Hotkey_Flag_RAlt},
    };
    if (exitChord.isActivatedBy(current)) {
        debug("exit hotkey, ralt-c, detected, ending program");
        service_stop();
        std::exit(1);
    }

    if (type == kCGEventKeyUp && lastTriggeredChord && lastTriggeredChord->keysym.keycode == keyCode) {
        lastTriggeredChord = std::nullopt;
    }

    if (type == kCGEventKeyDown && !isRepeat) {
        if (checkAndExecuteSequence(current)) return true;
    }

    for (const auto& [hotkey, command] : hotkeys) {
        if (hotkey.chords.size() > 1) continue;
        if (hotkey.chords[0].isActivatedBy(current)) {
            if ((!hotkey.on_release && type == kCGEventKeyDown || hotkey.on_release && type == kCGEventKeyUp) && (isRepeat && hotkey.repeat || !isRepeat)) {
                if (!command.empty()) {
                    debug("executing command: {}", command);
                    executeCommand(command);
                    if (type == kCGEventKeyDown) {
                        lastTriggeredChord = current;
                    }
                }
                if (hotkey.passthrough) return false;
                return true;
            }
        }
    }
    return false;
}

inline void KeyHandler::run() const {
    if (!runLoop) return;
    info("running key handler");
    CFRunLoopRun();
}

inline void KeyHandler::loadConfig(const std::string& config_file) {
    if (!file_exists(config_file)) error("config file not found");
    if (config_file.empty()) error("config file empty");

    info("config file set to: {}", config_file);
    std::ifstream file(config_file);
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    try {
        Parser parser(contents);
        ast::Program program = parser.parseProgram();
        Interpreter interpreter;
        auto result = interpreter.interpret(program);
        this->hotkeys = std::move(result.hotkeys);
        this->config = result.config;
    } catch (const std::exception& ex) {
        error("Error while parsing hotkeys: {}", ex.what());
    }
}
}  // namespace smhkd::key_handler
