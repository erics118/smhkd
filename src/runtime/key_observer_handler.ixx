module;

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <print>
#include <string>

export module key_observer_handler;

import chord;
import modifier;
import log;

export class KeyObserverHandler {
   private:
    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    // exit chord: ctrl + c
    static constexpr Chord EXIT_CHORD{
        .keysym = {.keycode = 8},
        .modifiers = {.flags = Hotkey_Flag_Control}};

    bool setupEventTap() {
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

    static CGEventRef eventCallback(
        CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* /*refcon*/) {
        if (handleKeyEvent(event, type)) return nullptr;
        return event;
    }

    static bool handleKeyEvent(CGEventRef event, CGEventType type) {
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

   public:
    bool init() {
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

    void run() const {
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
};
