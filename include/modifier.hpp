#pragma once

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <array>
#include <print>
#include <string>

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

    Hotkey_Flag_NX = (1 << 13),
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
inline const int nx_mod_offset = 13;

struct CustomModifier {
    std::string name;
    int flags;  // Combined flags of constituent modifiers

    std::strong_ordering operator<=>(const CustomModifier& other) const = default;
};

struct ModifierFlags {
    int flags;

    std::strong_ordering operator<=>(const ModifierFlags& other) const = default;

    [[nodiscard]] bool isActivatedBy(const ModifierFlags& other) const;
};

template <>
struct std::formatter<ModifierFlags> : std::formatter<std::string_view> {
    auto format(ModifierFlags flags, std::format_context& ctx) const {
        std::string str;
        if (flags.flags == 0) {
            return std::format_to(ctx.out(), "{}", str);
        }
        for (int i = 0; i < hotkey_flags.size(); i++) {
            if (flags.flags & hotkey_flags[i]) {
                str += hotkey_flag_names[i];
                str += " + ";
            }
        }
        str.pop_back();
        str.pop_back();
        str.pop_back();

        return std::format_to(ctx.out(), "{}", str);
    }
};

// check if a chord has a given modifier flag
[[nodiscard]] bool has_flags(const ModifierFlags& c, uint32_t flag);

// convert event CGEventFlags to ModifierFlags
[[nodiscard]] ModifierFlags eventModifierFlagsToHotkeyFlags(CGEventFlags flags);
