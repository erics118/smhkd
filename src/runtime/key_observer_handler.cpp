#include "key_observer_handler.hpp"

#include <array>
#include <optional>
#include <string_view>

#include "../common/log.hpp"

namespace {

struct ObservedModifierKey {
    uint32_t keycode;
    HotkeyFlag flag;
    std::string_view name;
};

constexpr std::array OBSERVED_MODIFIER_KEYS = {
    ObservedModifierKey{kVK_Shift, Hotkey_Flag_LShift, "lshift"},
    ObservedModifierKey{kVK_RightShift, Hotkey_Flag_RShift, "rshift"},
    ObservedModifierKey{kVK_Command, Hotkey_Flag_LCmd, "lcmd"},
    ObservedModifierKey{kVK_RightCommand, Hotkey_Flag_RCmd, "rcmd"},
    ObservedModifierKey{kVK_Option, Hotkey_Flag_LAlt, "lalt"},
    ObservedModifierKey{kVK_RightOption, Hotkey_Flag_RAlt, "ralt"},
    ObservedModifierKey{kVK_Control, Hotkey_Flag_LControl, "lctrl"},
    ObservedModifierKey{kVK_RightControl, Hotkey_Flag_RControl, "rctrl"},
    ObservedModifierKey{kVK_Function, Hotkey_Flag_Fn, "fn"},
};

std::optional<ObservedModifierKey> lookupObservedModifierKey(uint32_t keycode) {
    for (const auto& entry : OBSERVED_MODIFIER_KEYS) {
        if (entry.keycode == keycode) {
            return entry;
        }
    }
    return std::nullopt;
}

std::string describeObservedKey(const Chord& current, CGEventType type) {
    if (type == kCGEventFlagsChanged) {
        if (auto modifier = lookupObservedModifierKey(current.keysym.keycode)) {
            return std::string{modifier->name};
        }
    }
    return std::format("{}", current.keysym);
}

std::string describeObservedModifierTransition(const Chord& current, CGEventType type) {
    if (type != kCGEventFlagsChanged) {
        return "none";
    }
    if (auto modifier = lookupObservedModifierKey(current.keysym.keycode)) {
        return current.modifiers.has(modifier->flag) ? "pressed" : "released";
    }
    return "none";
}

std::string describeObservedChord(const Chord& current, CGEventType type) {
    if (type == kCGEventFlagsChanged && lookupObservedModifierKey(current.keysym.keycode)) {
        return std::format("{}", current.modifiers);
    }
    return std::format("{}", current);
}

}  // namespace

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

    const std::string key = describeObservedKey(current, type);
    const std::string transition = describeObservedModifierTransition(current, type);
    const std::string chord = describeObservedChord(current, type);

    std::print("\033[s\033[J\rkeycode: {:#02x}\nkey: {}\nmodifiers: {}\nchord: {}\nflags: {:032b}\nevent type: {}\nmodifier action: {}",
        current.keysym.keycode,
        key,
        current.modifiers,
        chord,
        current.modifiers.flags,
        eventType,
        transition);
    std::print("\033[u");

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
