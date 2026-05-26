#include "key_observer_handler.hpp"

#include "../common/log.hpp"

bool KeyObserverHandler::setupEventTap() {
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged);
    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, eventCallback, this);
    if (!tap) {
        return false;
    }
    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);
    eventTap = tap;
    CFRelease(runLoopSource);
    return true;
}

CGEventRef KeyObserverHandler::eventCallback(
    CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* /*refcon*/) {
    if (handleKeyEvent(event, type)) return nullptr;
    return event;
}

bool KeyObserverHandler::handleKeyEvent(CGEventRef event, CGEventType type) {
    auto keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    const CGEventFlags flags = CGEventGetFlags(event);

    Chord current{
        .keysym = {.keycode = keyCode},
        .modifiers = eventModifierFlagsToHotkeyFlags(flags),
    };

    auto exitChord = EXIT_CHORD;

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

bool KeyObserverHandler::init() {
    runLoop = CFRunLoopGetCurrent();
    if (!runLoop) {
        error("Failed to get current run loop");
        return false;
    }
    if (!setupEventTap()) {
        error("failed to setup event tap");
        return false;
    }
    return true;
}

void KeyObserverHandler::run() const {
    if (!runLoop) {
        error("run loop not initialized");
        return;
    }
    if (!eventTap) {
        error("event tap not initialized");
        return;
    }
    CFRunLoopRun();
}
