#pragma once

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <array>
#include <print>
#include <string>
#include <vector>

#include "locale.hpp"

inline const int key_has_implicit_fn_mod = 4;
inline const int key_has_implicit_nx_mod = 35;

// clang-format off
inline const std::array<std::string, 47> literal_keycode_str =
{
    "return",          "tab",             "space",
    "backspace",       "escape",          "delete",
    "home",            "end",             "pageup",
    "pagedown",        "insert",          "left",
    "right",           "up",              "down",
    "f1",              "f2",              "f3",
    "f4",              "f5",              "f6",
    "f7",              "f8",              "f9",
    "f10",             "f11",             "f12",
    "f13",             "f14",             "f15",
    "f16",             "f17",             "f18",
    "f19",             "f20",

    "sound_up",        "sound_down",      "mute",
    "play",            "previous",        "next",
    "rewind",          "fast",            "brightness_up",
    "brightness_down", "illumination_up", "illumination_down"
};

inline const std::array<uint32_t, 47> literal_keycode_value = {
    kVK_Return,     kVK_Tab,           kVK_Space,
    kVK_Delete,     kVK_Escape,        kVK_ForwardDelete,
    kVK_Home,       kVK_End,           kVK_PageUp,
    kVK_PageDown,   kVK_Help,          kVK_LeftArrow,
    kVK_RightArrow, kVK_UpArrow,       kVK_DownArrow,
    kVK_F1,         kVK_F2,            kVK_F3,
    kVK_F4,         kVK_F5,            kVK_F6,
    kVK_F7,         kVK_F8,            kVK_F9,
    kVK_F10,        kVK_F11,           kVK_F12,
    kVK_F13,        kVK_F14,           kVK_F15,
    kVK_F16,        kVK_F17,           kVK_F18,
    kVK_F19,        kVK_F20,

    NX_KEYTYPE_SOUND_UP,        NX_KEYTYPE_SOUND_DOWN,      NX_KEYTYPE_MUTE,
    NX_KEYTYPE_PLAY,            NX_KEYTYPE_PREVIOUS,        NX_KEYTYPE_NEXT,
    NX_KEYTYPE_REWIND,          NX_KEYTYPE_FAST,            NX_KEYTYPE_BRIGHTNESS_UP,
    NX_KEYTYPE_BRIGHTNESS_DOWN, NX_KEYTYPE_ILLUMINATION_UP, NX_KEYTYPE_ILLUMINATION_DOWN
};
// clang-format on

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
    KeyEventType eventType = KeyEventType::Down;
    bool passthrough{};
    bool repeat{};

    // optional vector of hotkeys
    std::vector<Hotkey> sequence;

    // call on the hotkey, ie hotkey.isActivatedBy(current)
    [[nodiscard]] bool isActivatedBy(const Hotkey& other) const;

    // only used internally for hashing for unordered_map
    bool operator==(const Hotkey& other) = delete;
};

template <>
struct std::formatter<Hotkey> : std::formatter<std::string_view> {
    auto format(Hotkey hk, std::format_context& ctx) const {
        // print out each part
        std::string str;

        // If this is part of a chord, format all hotkeys
        if (!hk.sequence.empty()) {
            for (size_t i = 0; i < hk.sequence.size(); i++) {
                if (i > 0) str += " ; ";
                str += std::format("{}", hk.sequence[i]);
            }
            return std::format_to(ctx.out(), "{}", str);
        }

        // Format single hotkey
        for (int i = 0; i < hotkey_flags.size(); i++) {
            if (hk.flags & hotkey_flags[i]) {
                str += hotkey_flag_names[i] + " + ";
            }
        }

        str += getNameOfKeycode(hk.keyCode) + " (" + std::to_string(hk.keyCode) + ")";

        std::format_to(
            ctx.out(), "{} ({}{}{})",
            str,
            hk.eventType,
            hk.passthrough ? " Passthrough" : "",
            hk.repeat ? " Repeat" : ""
            // hk.command.size() > 0 ? (": `" + hk.command + "`") : ""
        );

        return ctx.out();
    }
};

int eventModifierFlagsToHotkeyFlags(CGEventFlags flags);

template <>
struct std::hash<KeyEventType> {
    std::size_t operator()(const KeyEventType& et) const {
        return std::hash<int>{}(static_cast<int>(et));
    }
};

template <>
struct std::hash<Hotkey> {
    std::size_t operator()(const Hotkey& hk) const {
        std::size_t h1 = std::hash<int>{}(hk.flags);
        std::size_t h2 = std::hash<uint32_t>{}(hk.keyCode);
        std::size_t h3 = std::hash<bool>{}(hk.passthrough);
        std::size_t h4 = std::hash<bool>{}(hk.repeat);
        std::size_t h5 = std::hash<KeyEventType>{}(hk.eventType);

        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
    }
};

template <>
struct std::equal_to<Hotkey> {
    bool operator()(const Hotkey& a, const Hotkey& b) const {
        return a.isActivatedBy(b) && b.isActivatedBy(a);
    }
};
