#include "key_observer_handler.hpp"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <cstdlib>

#include "log.hpp"

bool KeyObserverHandler::init() {
    // Get the main run loop
    runLoop = CFRunLoopGetCurrent();
    if (!runLoop) {
        return false;
    }

    // Set up event tap
    if (!setupEventTap()) {
        return false;
    }

    return true;
}

bool KeyObserverHandler::setupEventTap() {
    // Create an event tap to monitor keyboard events
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged);

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

CGEventRef KeyObserverHandler::eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
    auto* keyHandler = static_cast<KeyObserverHandler*>(refcon);

    // Handle keyboard events
    if (keyHandler->handleKeyEvent(event, type)) {
        return nullptr;  // Consume the event
    }

    return event;  // Let it pass through
}

bool KeyObserverHandler::handleKeyEvent(CGEventRef event, CGEventType type) {
    auto keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    CGEventFlags flags = CGEventGetFlags(event);
    bool isRepeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

    Chord current{
        .keysym = {
            .keycode = keyCode,
        },
        .modifiers = eventModifierFlagsToHotkeyFlags(flags),
    };

    // Special handling for to exit
    auto exitChord = Chord{
        .keysym = {
            .keycode = 8,
        },
        .modifiers = {
            .flags = Hotkey_Flag_Control,
        },
    };

    if (exitChord.isActivatedBy(current)) {
        exit(1);
    }

    std::string eventType;
    switch (type) {
        case kCGEventKeyDown:
            eventType = "kCGEventKeyDown";
            break;
        case kCGEventKeyUp:
            eventType = "kCGEventKeyUp";
            break;
        case kCGEventFlagsChanged:
            eventType = "kCGEventFlagsChanged";
            break;
        default:
            eventType = "unknown";
            break;
    }

    std::print("\033[s\033[J\rkey: {}\nkeycode: {:#02x}\nmodifiers: {}\nflags: {:032b}\nevent type: {}\033[u",
        current.keysym,
        current.keysym.keycode,
        current.modifiers,
        current.modifiers.flags,
        eventType);

    return true;
}

void KeyObserverHandler::run() const {
    if (!runLoop) {
        return;
    }

    CFRunLoopRun();
}
