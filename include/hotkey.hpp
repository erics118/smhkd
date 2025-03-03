#pragma once

#include <CoreGraphics/CGEventTypes.h>

#include <array>
#include <print>
#include <string>

#include "locale.hpp"

enum HotkeyFlag {
    Hotkey_Flag_Alt = (1 << 0),
    Hotkey_Flag_LAlt = (1 << 1),
    Hotkey_Flag_RAlt = (1 << 2),

    Hotkey_Flag_Shift = (1 << 3),
    Hotkey_Flag_LShift = (1 << 4),
    Hotkey_Flag_RShift = (1 << 5),

    Hotkey_Flag_Cmd = (1 << 6),
    Hotkey_Flag_LCmd = (1 << 7),
    Hotkey_Flag_RCmd = (1 << 8),

    Hotkey_Flag_Control = (1 << 9),
    Hotkey_Flag_LControl = (1 << 10),
    Hotkey_Flag_RControl = (1 << 11),
    Hotkey_Flag_Fn = (1 << 12),
};

inline const int l_offset = 1;
inline const int r_offset = 2;

inline const std::array<int, 13> cgevent_flags = {
    kCGEventFlagMaskAlternate,
    NX_DEVICELALTKEYMASK,
    NX_DEVICERALTKEYMASK,

    kCGEventFlagMaskShift,
    NX_DEVICELSHIFTKEYMASK,
    NX_DEVICERSHIFTKEYMASK,

    kCGEventFlagMaskCommand,
    NX_DEVICELCMDKEYMASK,
    NX_DEVICERCMDKEYMASK,

    kCGEventFlagMaskControl,
    NX_DEVICELCTLKEYMASK,
    NX_DEVICERCTLKEYMASK,

    kCGEventFlagMaskSecondaryFn,
};

inline const std::array<HotkeyFlag, 13> hotkey_flags = {
    Hotkey_Flag_Alt,
    Hotkey_Flag_LAlt,
    Hotkey_Flag_RAlt,

    Hotkey_Flag_Shift,
    Hotkey_Flag_LShift,
    Hotkey_Flag_RShift,

    Hotkey_Flag_Cmd,
    Hotkey_Flag_LCmd,
    Hotkey_Flag_RCmd,

    Hotkey_Flag_Control,
    Hotkey_Flag_LControl,
    Hotkey_Flag_RControl,

    Hotkey_Flag_Fn,
};

inline const std::array<std::string, 13> hotkey_flag_names = {
    "alt",
    "lalt",
    "ralt",

    "shift",
    "lshift",
    "rshift",

    "cmd",
    "lcmd",
    "rcmd",

    "ctrl",
    "lctrl",
    "rctrl",

    "fn",
};

inline const int alt_mod_offset = 0;
inline const int shift_mod_offset = 3;
inline const int cmd_mod_offset = 6;
inline const int ctrl_mod_offset = 9;
inline const int fn_mod_offset = 12;

enum class KeyEventType {
    Down,  // default
    Up,
    Both,
};

// Specialize std::formatter for KeyEventType
template <>
struct std::formatter<KeyEventType> : std::formatter<std::string_view> {
    auto format(KeyEventType et, std::format_context& ctx) const {
        std::string_view name;
        switch (et) {
            case KeyEventType::Down: name = "Down"; break;
            case KeyEventType::Up: name = "Up"; break;
            case KeyEventType::Both: name = "Both"; break;
            default: name = "Unknown";
        }
        return std::format_to(ctx.out(), "{}", name);
    }
};

struct CustomModifier {
    std::string name;
    int flags;  // Combined flags of constituent modifiers
};

struct Hotkey {
    int flags{};
    uint32_t keyCode{};
    std::string command;
    KeyEventType eventType = KeyEventType::Down;
    bool passthrough{};
    bool repeat{};

    bool operator==(const Hotkey& other) const;
};

template <>
struct std::formatter<Hotkey> : std::formatter<std::string_view> {
    auto format(Hotkey hk, std::format_context& ctx) const {
        // print out each part

        std::string str = getNameOfKeycode(hk.keyCode);

        for (int i = 0; i < hotkey_flags.size(); i++) {
            if (hk.flags & hotkey_flags[i]) {
                str += " + " + hotkey_flag_names[i];
            }
        }

        std::format_to(
            ctx.out(), "{} ({}{}{}): {}",
            str,
            hk.eventType,
            hk.passthrough ? " Passthrough" : "",
            hk.repeat ? " Repeat" : "",
            hk.command);

        return ctx.out();
    }
};

int eventModifierFlagsToHotkeyFlags(CGEventFlags flags);
