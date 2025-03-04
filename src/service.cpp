#include "service.hpp"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
#include <cstdlib>

#include "log.hpp"
#include "utils.hpp"

// Maximum time between chord presses (in seconds)
// TODO: make this configurable
constexpr double MAX_CHORD_INTERVAL = 3.0;

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

void Service::clearSequence() {
    currentSequence.clear();
    lastKeyPressTime = 0;
}

bool Service::checkAndExecuteSequence(const Hotkey& current) {
    // First, check timing
    auto now = std::chrono::system_clock::now();
    double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();

    if (lastKeyPressTime > 0 && (currentTime - lastKeyPressTime) > MAX_CHORD_INTERVAL) {
        debug("Chord sequence timed out, clearing");
        clearSequence();
        return false;
    }

    lastKeyPressTime = currentTime;

    currentSequence.push_back(current);

    // check hotkeys with sequences
    for (const auto& hotkey : hotkeys) {
        if (hotkey.sequence.empty()) {
            continue;
        }

        // Check if our current sequence matches this hotkey's sequence
        if (currentSequence.size() > hotkey.sequence.size()) {
            continue;  // Too long to match
        }

        // Check if what we have so far matches
        bool matches = true;
        for (size_t i = 0; i < currentSequence.size(); i++) {
            if (!(hotkey.sequence[i] == currentSequence[i])) {
                matches = false;
                break;
            }
        }

        if (!matches) {
            continue;
        }

        // If we've matched the complete sequence
        if (currentSequence.size() == hotkey.sequence.size()) {
            debug("Matched complete chord sequence");
            if (!hotkey.command.empty()) {
                debug("executing command: {}", hotkey.command);
                executeCommand(hotkey.command);
            }
            clearSequence();
            return true;
        }

        // If we've matched part of a sequence, keep collecting
        debug("Matched partial chord sequence");
        return true;
    }

    // If we get here, current sequence doesn't match any prefix
    debug("Chord sequence doesn't match any prefix, clearing");
    clearSequence();
    return false;
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

    // Only process key down events for sequences
    if (type == kCGEventKeyDown && !isRepeat) {
        if (checkAndExecuteSequence(current)) {
            return true;  // Consume the event if it's part of a sequence
        }
    }

    // Check for single hotkeys
    for (const auto& hotkey : hotkeys) {
        // Skip sequence hotkeys
        if (!hotkey.sequence.empty()) {
            continue;
        }

        if (hotkey == current) {
            if ((hotkey.eventType == KeyEventType::Both || hotkey.eventType == current.eventType)
                && (isRepeat && hotkey.repeat || !isRepeat)) {
                debug("consumed");
                if (!hotkey.command.empty()) {
                    debug("executing command: {}", hotkey.command);
                    executeCommand(hotkey.command);

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
