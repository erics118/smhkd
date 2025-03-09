#include "key_handler.hpp"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
#include <cstdlib>
#include <fstream>

#include "log.hpp"
#include "parser.hpp"
#include "service.hpp"
#include "utils.hpp"

bool KeyHandler::init() {
    // Get the main run loop
    runLoop = CFRunLoopGetCurrent();
    if (!runLoop) {
        return false;
    }

    info("run loop initialized");

    // Set up event tap
    if (!setupEventTap()) {
        return false;
    }

    debug("event tap initialized");

    return true;
}

bool KeyHandler::setupEventTap() {
    // Create an event tap to monitor keyboard events
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);

    CFMachPortRef tap = CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
        eventMask, eventCallback, this);

    if (!tap) {
        error("failed to create event tap");
    }

    // Create a run loop source and add it to the run loop
    CFRunLoopSourceRef runLoopSource =
        CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);

    CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopCommonModes);

    // Enable the event tap
    CGEventTapEnable(tap, true);

    eventTap = tap;
    CFRelease(runLoopSource);

    return true;
}

CGEventRef KeyHandler::eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
    auto* keyHandler = static_cast<KeyHandler*>(refcon);

    // Handle keyboard events
    if (keyHandler->handleKeyEvent(event, type)) {
        return nullptr;  // Consume the event
    }

    return event;  // Let it pass through
}

void KeyHandler::clearSequence() {
    debug("clearing sequence");
    currentChords.clear();
    lastKeyPressTime = std::chrono::time_point<std::chrono::system_clock>::min();
}

void KeyHandler::clearSimultaneousKeys() {
    debug("clearing simultaneous keys");
    simultaneousKeys.clear();
    firstSimultaneousKeyTime = std::chrono::time_point<std::chrono::system_clock>::min();
}

bool KeyHandler::checkAndExecuteSequence(const Chord& current) {
    // TODO: use mach for precise timing
    // https://developer.apple.com/library/archive/technotes/tn2169/_index.html#//apple_ref/doc/uid/DTS40013172-CH1-TNTAG2000
    auto now = std::chrono::system_clock::now();

    // if difference in milliseconds between now and lastKeyPressTime is greater
    // than maxChordInterval, clear the sequence
    if (now != std::chrono::time_point<std::chrono::system_clock>::min()
        && now - lastKeyPressTime > config.maxChordInterval) {
        debug("Chord sequence timed out, clearing");
        clearSequence();
        return false;
    }

    lastKeyPressTime = now;

    currentChords.push_back(current);

    // check hotkeys with sequences
    for (const auto& [hotkey, command] : hotkeys) {
        if (hotkey.chords.size() == 1) {
            continue;
        }

        // Check if our current sequence matches this hotkey's sequence
        if (currentChords.size() > hotkey.chords.size()) {
            continue;  // Too long to match
        }

        // Check if what we have so far matches
        bool matches = true;
        for (size_t i = 0; i < currentChords.size(); i++) {
            if (!(hotkey.chords[i].isActivatedBy(currentChords[i]))) {
                matches = false;
                break;
            }
        }

        if (!matches) {
            continue;
        }

        // If we've matched the complete sequence
        if (currentChords.size() == hotkey.chords.size()) {
            debug("Matched complete chord sequence");
            if (!command.empty()) {
                debug("executing command: {}", command);
                executeCommand(command);
            }
            clearSequence();
            return true;
        }

        // If we've matched part of a sequence, keep collecting
        debug("Matched partial chord sequence");
        return true;
    }

    // If we get here, current sequence doesn't match any prefix
    // debug("Chord sequence doesn't match any prefix, clearing");
    clearSequence();
    return false;
}

bool KeyHandler::handleKeyEvent(CGEventRef event, CGEventType type) {
    Chord current{
        .keysym = {
            .keycodes = {static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode))},
        },
        .modifiers = eventModifierFlagsToHotkeyFlags(CGEventGetFlags(event)),
    };

    bool isRepeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

    // Special handling for to exit
    auto exitChord = Chord{
        .keysym = {
            .keycodes = {8},
        },
        .modifiers = {
            .flags = Hotkey_Flag_RAlt,
        },
    };

    if (exitChord.isActivatedBy(current)) {
        debug("exit hotkey, ralt-c, detected, ending program");
        service_stop();
        exit(1);
    }

    debug("handling event: {} {}", current, type == kCGEventKeyDown ? "" : "(Release)");

    // Clear last triggered chord on key up
    if (type == kCGEventKeyUp && lastTriggeredChord && lastTriggeredChord->keysym == current.keysym) {
        lastTriggeredChord = std::nullopt;
        // debug("cleared last triggered chord");
    }

    // Handle simultaneous keys
    if (type == kCGEventKeyDown && !isRepeat) {
        auto now = std::chrono::system_clock::now();

        // If this is the first key in a potential simultaneous sequence
        if (simultaneousKeys.empty()) {
            debug("first simultaneous key detected: {}", current.keysym);
            simultaneousKeys.push_back(current.keysym);
            firstSimultaneousKeyTime = now;

            for (const auto& key : simultaneousKeys) {
                debug("simultaneous key: {}", key);
            }
            return true;
        } else {
            // Check if this key was pressed within the simultaneous threshold
            debug("simultaneous key detected: {}", current.keysym);
            debug("first simultaneous key time: {}", firstSimultaneousKeyTime);
            debug("now: {}", now);
            debug("simultaneous threshold: {}", config.simultaneousThreshold);
            debug("difference (ms): {}", std::chrono::duration_cast<std::chrono::milliseconds>(now - firstSimultaneousKeyTime).count());
            if (now - firstSimultaneousKeyTime <= config.simultaneousThreshold) {
                debug("simultaneous key detected: {}", current.keysym);
                simultaneousKeys.push_back(current.keysym);

                // Check for simultaneous hotkeys
                for (const auto& [hotkey, command] : hotkeys) {
                    if (!hotkey.chords.back().isSimultaneous()) {
                        continue;
                    }

                    // Check if all required keys are pressed
                    bool allKeysPressed = true;
                    for (const auto& keycode : hotkey.chords[0].keysym.keycodes) {
                        bool keyFound = false;
                        for (const auto& simKey : simultaneousKeys) {
                            if (simKey.keycodes.contains(keycode)) {
                                keyFound = true;
                                break;
                            }
                        }
                        if (!keyFound) {
                            allKeysPressed = false;
                            break;
                        }
                    }

                    if (allKeysPressed) {
                        if (!command.empty()) {
                            debug("executing simultaneous command: {}", command);
                            executeCommand(command);
                        }
                        clearSimultaneousKeys();
                        return true;  // Consume the event
                    } else {
                        debug("not all keys pressed for simultaneous hotkey");
                        debug("waiting for keys to be pressed");
                        return true;
                    }
                }
            } else {
                // Outside threshold, treat as new sequence
                debug("simultaneous key outside threshold, treating as new sequence");
                clearSimultaneousKeys();
                simultaneousKeys.push_back(current.keysym);
                firstSimultaneousKeyTime = now;
            }
        }
    }

    // Only process key down events for sequences
    if (type == kCGEventKeyDown && !isRepeat) {
        if (checkAndExecuteSequence(current)) {
            return true;  // Consume the event if it's part of a sequence
        }
    }

    // Check for single hotkeys
    for (const auto& [hotkey, command] : hotkeys) {
        // Skip sequence hotkeys and simultaneous hotkeys
        if (hotkey.chords.back().isSimultaneous()) {
            continue;
        }

        if (hotkey.chords[0].isActivatedBy(current)) {
            if ((!hotkey.on_release && type == kCGEventKeyDown || hotkey.on_release && type == kCGEventKeyUp)
                && (isRepeat && hotkey.repeat || !isRepeat)) {
                if (!command.empty()) {
                    debug("executing command: {}", command);
                    executeCommand(command);

                    // Store this hotkey as the last triggered one if it's a key down event
                    if (type == kCGEventKeyDown) {
                        lastTriggeredChord = current;
                    }
                }
                if (hotkey.passthrough) {
                    return false;  // Let the event pass through
                }
                return true;  // Consume the event
            }
        }
    }

    return false;  // Let all other events pass through
}

void KeyHandler::run() const {
    if (!runLoop) {
        return;
    }

    // Run the main loop
    info("running key handler");

    CFRunLoopRun();
}

void KeyHandler::loadConfig(const std::string& config_file) {
    if (config_file.empty() || !file_exists(config_file)) {
        error("config file not found");
    }

    info("config file set to: {}", config_file);

    std::ifstream file(config_file);

    // read entire file into string
    std::string configFileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    try {
        Parser parser(configFileContents);

        auto hotkeys = parser.parseFile();

        ConfigProperties config = parser.getConfigProperties();

        this->hotkeys = hotkeys;
        this->config = config;
    } catch (const std::exception& ex) {
        error("Error while parsing hotkeys: {}", ex.what());
    }
}
