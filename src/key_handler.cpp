#include "key_handler.hpp"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
#include <cstdlib>

#include "log.hpp"
#include "utils.hpp"

// Maximum time between chord presses (in seconds)
// TODO: make this configurable
constexpr double MAX_CHORD_INTERVAL = 3.0;

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

CGEventRef KeyHandler::eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
    auto* keyHandler = static_cast<KeyHandler*>(refcon);

    // Handle keyboard events
    if (keyHandler->handleKeyEvent(event, type)) {
        return nullptr;  // Consume the event
    }

    return event;  // Let it pass through
}

void KeyHandler::clearSequence() {
    currentChords.clear();
    lastKeyPressTime = 0;
}

bool KeyHandler::checkAndExecuteSequence(const Chord& current) {
    // TODO: use mach for precise timing
    // https://developer.apple.com/library/archive/technotes/tn2169/_index.html#//apple_ref/doc/uid/DTS40013172-CH1-TNTAG2000
    auto now = std::chrono::system_clock::now();
    double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();

    if (lastKeyPressTime > 0 && (currentTime - lastKeyPressTime) > MAX_CHORD_INTERVAL) {
        // debug("Chord sequence timed out, clearing");
        clearSequence();
        return false;
    }

    lastKeyPressTime = currentTime;

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
            .flags = Hotkey_Flag_RAlt,
        },
    };

    if (exitChord.isActivatedBy(current)) {
        debug("exit hotkey, ralt-c, detected, ending program");
        exit(0);
    }

    debug("handling event: {} {}", current, type == kCGEventKeyDown ? "" : "(Release)");

    // Clear last triggered chord on key up
    if (type == kCGEventKeyUp && lastTriggeredChord && lastTriggeredChord->keysym.keycode == keyCode) {
        lastTriggeredChord = std::nullopt;
        // debug("cleared last triggered chord");
    }

    // Only process key down events for sequences
    if (type == kCGEventKeyDown && !isRepeat) {
        if (checkAndExecuteSequence(current)) {
            return true;  // Consume the event if it's part of a sequence
        }
    }

    // Check for single hotkeys
    for (const auto& [hotkey, command] : hotkeys) {
        // Skip sequence hotkeys
        if (hotkey.chords.size() > 1) {
            continue;
        }

        if (hotkey.chords[0].isActivatedBy(current)) {
            if ((!hotkey.on_release && type == kCGEventKeyDown || hotkey.on_release && type == kCGEventKeyUp)
                && (isRepeat && hotkey.repeat || !isRepeat)) {
                // debug("consumed");
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

    debug("watching for shortcuts");

    // Run the main loop
    info("running key handler");

    CFRunLoopRun();
}
