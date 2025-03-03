#include "service.hpp"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <cstdlib>

#include "log.hpp"

bool Service::init() {
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

bool Service::setupEventTap() {
    // Request accessibility permissions
    if (!AXIsProcessTrusted()) {
        error("need to grant accessibility permissions to this application");
        return false;
    }

    // Create an event tap to monitor keyboard events
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);

    CFMachPortRef tap = CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
        eventMask, eventCallback, this);

    if (!tap) {
        error("failed to create event tap");
        return false;
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

CGEventRef Service::eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
    auto* service = static_cast<Service*>(refcon);

    // Handle keyboard events
    if (service->handleKeyEvent(event, type)) {
        return nullptr;  // Consume the event
    }

    return event;  // Let it pass through
}

bool Service::handleKeyEvent(CGEventRef event, CGEventType type) {
    auto keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    CGEventFlags flags = CGEventGetFlags(event);
    bool isRepeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

    // Special handling for Ctrl-C
    if (keyCode == 8 && (flags & kCGEventFlagMaskControl) && !(flags & (kCGEventFlagMaskCommand | kCGEventFlagMaskAlternate | kCGEventFlagMaskShift))) {
        debug("ctrl-c detected, ending program");
        exit(0);
    }

    Hotkey current;
    int flags_from_event = eventModifierFlagsToHotkeyFlags(flags);

    current.flags = flags_from_event;
    current.keyCode = keyCode;
    current.eventType = (type == kCGEventKeyDown) ? KeyEventType::Down : KeyEventType::Up;

    debug("handling hotkey: {}", current);

    // Clear last triggered hotkey on key up
    if (type == kCGEventKeyUp && lastTriggeredHotkey && lastTriggeredHotkey->keyCode == keyCode) {
        lastTriggeredHotkey = std::nullopt;
        debug("cleared last triggered hotkey");
    }

    for (const auto& hotkey : hotkeys) {
        if (hotkey == current) {
            if ((hotkey.eventType == KeyEventType::Both || hotkey.eventType == current.eventType)
                && (isRepeat && hotkey.repeat || !isRepeat)) {
                debug("consumed");
                if (!hotkey.command.empty()) {
                    debug("executing command: {}", hotkey.command);
                    system(hotkey.command.c_str());

                    // Store this hotkey as the last triggered one if it's a key down event
                    if (type == kCGEventKeyDown) {
                        lastTriggeredHotkey = current;
                        lastTriggeredHotkey->command = hotkey.command;
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

void Service::run() const {
    if (!runLoop) {
        return;
    }

    debug("watching for shortcuts");

    // Run the main loop
    info("running service");

    CFRunLoopRun();
}
