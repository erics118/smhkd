module;

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <print>
#include <string>

export module smhkd.key_observer_handler;

import smhkd.chord;
import smhkd.modifier;
import smhkd.log;

export namespace smhkd::key_observer_handler {
struct KeyObserverHandler {
    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    bool init();
    void run() const;
    bool setupEventTap();
    [[nodiscard]] static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon);
    [[nodiscard]] bool handleKeyEvent(CGEventRef event, CGEventType type);
};

inline bool KeyObserverHandler::init() {
    runLoop = CFRunLoopGetCurrent();
    if (!runLoop) return false;
    if (!setupEventTap()) return false;
    return true;
}

inline bool KeyObserverHandler::setupEventTap() {
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged);
    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, eventCallback, this);
    if (!tap) error("failed to create event tap");
    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);
    eventTap = tap;
    CFRelease(runLoopSource);
    return true;
}

inline CGEventRef KeyObserverHandler::eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
    auto* keyHandler = static_cast<KeyObserverHandler*>(refcon);
    if (keyHandler->handleKeyEvent(event, type)) return nullptr;
    return event;
}

inline bool KeyObserverHandler::handleKeyEvent(CGEventRef event, CGEventType type) {
    auto keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    CGEventFlags flags = CGEventGetFlags(event);

    Chord current{
        .keysym = {.keycode = keyCode},
        .modifiers = eventModifierFlagsToHotkeyFlags(flags),
    };
    auto exitChord = Chord{
        .keysym = {.keycode = 8},
        .modifiers = {.flags = Hotkey_Flag_Control},
    };
    if (exitChord.isActivatedBy(current)) {
        std::exit(1);
    }

    std::string eventType;
    switch (type) {
        case kCGEventKeyDown: eventType = "kCGEventKeyDown"; break;
        case kCGEventKeyUp: eventType = "kCGEventKeyUp"; break;
        case kCGEventFlagsChanged: eventType = "kCGEventFlagsChanged"; break;
        default: eventType = "unknown"; break;
    }

    std::print("\033[s\033[J\rkeycode: {:#02x}\nflags: {:032b}\nevent type: {}\033[u",
        current.keysym.keycode,
        current.modifiers.flags,
        eventType);

    return true;
}

inline void KeyObserverHandler::run() const {
    if (!runLoop) return;
    CFRunLoopRun();
}
}  // namespace smhkd::key_observer_handler
