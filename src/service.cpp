#include "service.hpp"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
#include <cstdlib>

#include "keysym.hpp"
#include "log.hpp"
#include "utils.hpp"

// Maximum time between chord presses (in seconds)
// TODO: make this configurable
constexpr double MAX_CHORD_INTERVAL = 3.0;

// key must be pressed for this long to be considered held,
// and thus eligible for being used as a modifier
// if it is pressed for a shorter time, it is considered a tap, and
// ineligible for being used as a modifier, and will be re-synthesized
constexpr auto HELD_THRESHOLD = std::chrono::milliseconds(100);

// key must be pressed and released within this time to be considered a tap, and
// thus ineligible for being used as a modifier

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
    currentChords.clear();
    lastKeyPressTime = 0;
}

bool Service::checkAndExecuteSequence(const Chord& current) {
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

bool Service::handleKeyEvent(CGEventRef event, CGEventType type) {
    // Get the event source
    auto sourceStateID = CGEventGetIntegerValueField(event, kCGEventSourceStateID);

    // Ignore events we generated
    if (sourceStateID == 0) {  // Our synthetic events will have sourceStateID = 0
        return false;
    }

    bool isRepeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

    Chord current{
        .keysym = {
            .keycode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode)),
        },
        .modifiers = {
            .flags = eventModifierFlagsToHotkeyFlags(CGEventGetFlags(event)),
        },
    };

    // Block repeats for any key that's a potential held modifier
    if (isRepeat && isPotentialHeldModifier(current.keysym)) {
        return true;  // Consume repeat events for held modifiers
    }

    // Process any delayed events that have exceeded the threshold
    // commented out bc unwanted behavior
    processDelayedEvents();
    // TODO: need to process delayed events later in the function

    // Handle key up events
    if (type == kCGEventKeyUp) {
        // Remove from held keys
        heldKeysyms.erase(current.keysym);

        // If this was a delayed key that was used as a modifier, just consume it
        auto it = delayedKeysyms.find(current.keysym);
        if (it != delayedKeysyms.end()) {
            bool wasConsumed = it->second.consumed;
            auto pressTime = it->second.pressTime;
            auto now = std::chrono::steady_clock::now();

            // Clean up
            delayedKeysyms.erase(it);

            // If it was a quick tap (< 100ms), synthesize both down and up events
            if (!wasConsumed && (now - pressTime) < HELD_THRESHOLD) {
                debug("sending delayed event because it was a tap: {}", current.keysym);
                // Create synthetic events with null source (will have sourceStateID = 0)
                CGEventRef syntheticDown = CGEventCreateKeyboardEvent(nullptr, current.keysym.keycode, true);
                CGEventPost(kCGSessionEventTap, syntheticDown);
                CFRelease(syntheticDown);

                CGEventRef syntheticUp = CGEventCreateKeyboardEvent(nullptr, current.keysym.keycode, false);
                CGEventPost(kCGSessionEventTap, syntheticUp);
                CFRelease(syntheticUp);

                return true;  // Consume the original up event
            }

            if (wasConsumed) {
                return true;  // Consume the up event for consumed modifiers
            }
        }
    }

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
    if (type == kCGEventKeyUp && lastTriggeredChord && lastTriggeredChord->keysym == current.keysym) {
        lastTriggeredChord = std::nullopt;
    }

    // For key down events
    if (type == kCGEventKeyDown && !isRepeat) {
        // If this could be a held modifier, delay the event
        if (isPotentialHeldModifier(current.keysym)) {
            delayedKeysyms[current.keysym] = DelayedKeyEvent{
                .pressTime = std::chrono::steady_clock::now(),
                .consumed = false,
            };
            debug("delaying event: {}", getNameOfKeysym(current.keysym));
            return true;  // Consume the event for now
        }

        // Update held keys tracking for non-delayed events
        heldKeysyms[current.keysym] = std::chrono::steady_clock::now();

        // Check sequences
        if (checkAndExecuteSequence(current)) {
            return true;
        }
    }

    // Check for single hotkeys
    for (const auto& [hotkey, command] : hotkeys) {
        // Skip sequence hotkeys
        if (hotkey.chords.size() > 1) {
            continue;
        }

        // Check if any held modifiers match
        bool heldModifiersMatch = true;
        for (const auto& held : hotkey.chords[0].held_modifiers) {
            if (!isKeyHeldAsModifier(held)) {
                heldModifiersMatch = false;
                break;
            }
        }

        if (!heldModifiersMatch) {
            continue;
        }

        // Mark the held keys as consumed
        for (const auto& held : hotkey.chords[0].held_modifiers) {
            auto it = delayedKeysyms.find(held);
            if (it != delayedKeysyms.end()) {
                it->second.consumed = true;
            }
        }

        if (hotkey.chords[0].isActivatedBy(current)) {
            if ((!hotkey.on_release && type == kCGEventKeyDown || hotkey.on_release && type == kCGEventKeyUp)
                && (isRepeat && hotkey.repeat || !isRepeat)) {
                if (!command.empty()) {
                    debug("executing command: {}", command);
                    executeCommand(command);

                    if (type == kCGEventKeyDown) {
                        lastTriggeredChord = current;
                    }
                }
                if (hotkey.passthrough) {
                    return false;
                }
                return true;
            }
        }
    }

    return false;
}

bool Service::isKeyHeldAsModifier(Keysym keysym) const {
    auto now = std::chrono::steady_clock::now();
    auto it = heldKeysyms.find(keysym);
    if (it == heldKeysyms.end()) {
        return false;
    }

    // Consider a key held if it's been pressed for at least 100ms
    return (now - it->second) >= HELD_THRESHOLD;
}

bool Service::isPotentialHeldModifier(Keysym keysym) const {
    // Check if this key is used as a held modifier in any hotkey
    for (const auto& [hotkey, _] : hotkeys) {
        for (const auto& chord : hotkey.chords) {
            for (const auto& held : chord.held_modifiers) {
                if (held == keysym) {
                    return true;
                }
            }
        }
    }
    return false;
}

void Service::processDelayedEvents() {
    auto now = std::chrono::steady_clock::now();

    // Find events that have exceeded the threshold
    std::vector<Keysym> toProcess;
    for (const auto& [keysym, delayed] : delayedKeysyms) {
        if ((now - delayed.pressTime) >= HELD_THRESHOLD) {
            toProcess.push_back(keysym);
        }
    }

    // Process the events
    for (Keysym keysym : toProcess) {
        sendDelayedEvent(keysym);
    }
}

void Service::sendDelayedEvent(Keysym keysym) {
    auto it = delayedKeysyms.find(keysym);

    if (it == delayedKeysyms.end()) {
        return;
    }

    // If the key wasn't used as a modifier, synthesize and send new key down event
    if (!it->second.consumed) {
        debug("sending delayed event because it was held but not used as a hold modifier: {}", keysym);
        // Create synthetic event with null source (will have sourceStateID = 0)
        CGEventRef syntheticEvent = CGEventCreateKeyboardEvent(nullptr, keysym.keycode, true);
        CGEventPost(kCGSessionEventTap, syntheticEvent);
        CFRelease(syntheticEvent);

        // Add to held keys with original timestamp
        heldKeysyms[keysym] = it->second.pressTime;
    }

    delayedKeysyms.erase(it);
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
