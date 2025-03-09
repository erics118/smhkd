#pragma once

#include <Carbon/Carbon.h>
#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <array>
#include <print>
#include <string>

#include "locale.hpp"

struct Keysym {
    uint32_t keycode;

    std::strong_ordering operator<=>(const Keysym& other) const = default;
};

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

int getImplicitFlags(const std::string& literal);

template <>
struct std::formatter<Keysym> : std::formatter<std::string_view> {
    auto format(const Keysym& keysym, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", getNameOfKeycode(keysym.keycode));
    }
};
