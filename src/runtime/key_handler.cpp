#include "key_handler.hpp"

#include "../common/cf_string.hpp"
#include "../common/log.hpp"
#include "../lang/config_loader.hpp"
#include "../runtime/service.hpp"

bool KeyHandler::init() {
    runLoop = CFRunLoopGetCurrent();
    if (!runLoop) return false;
    debug("run loop initialized");
    if (!setupEventTap()) return false;
    debug("event tap initialized");
    return true;
}

bool KeyHandler::setupEventTap() {
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

CGEventRef KeyHandler::eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
    auto* keyHandler = static_cast<KeyHandler*>(refcon);
    if (keyHandler->handleKeyEvent(event, type)) return nullptr;
    return event;
}

bool KeyHandler::handleKeyEvent(CGEventRef event, CGEventType type) {
    if (CGEventGetIntegerValueField(event, kCGEventSourceUserData) == HotkeyEngine::SYNTHETIC_REMAP_TAG) {
        return false;
    }

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
        info("exit hotkey, ralt-c, detected, ending program");
        service_stop();
        std::exit(1);
    }

    return engine.handleEvent(current, type, isRepeat, getFrontProcessName());
}

std::string KeyHandler::getFrontProcessName() {
    ProcessSerialNumber psn{};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (GetFrontProcess(&psn) != noErr) {
        return "";
    }
    CFStringRef cfName{};
    if (CopyProcessName(&psn, &cfName) != noErr || !cfName) {
        return "";
    }
#pragma clang diagnostic pop

    std::string result = cfStringToString(cfName);
    CFRelease(cfName);
    return result;
}

void KeyHandler::run() const {
    if (!runLoop) return;
    info("running key handler");
    CFRunLoopRun();
}

void KeyHandler::loadConfig(const std::string& config_file) {
    info("config file set to: {}", config_file);
    auto result = ConfigLoader::loadFromFile(config_file);
    if (result.fileError) {
        warn("config error: {}", *result.fileError);
    }
    for (const auto& parse_error : result.parseErrors) {
        warn("parse error at line {}, column {}: {}", parse_error.row, parse_error.col, parse_error.message);
    }
    for (const auto& interpreter_error : result.interpreterErrors) {
        warn("config error: {}", interpreter_error.message);
    }
    engine.applyConfig(std::move(result.hotkeys), std::move(result.remaps), std::move(result.config));
}
