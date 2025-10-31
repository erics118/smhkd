module;

#include <Carbon/Carbon.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <algorithm>
#include <array>
#include <compare>
#include <optional>
#include <print>
#include <string>

export module smhkd.keysym;
import smhkd.modifier;

export struct Keysym {
    uint32_t keycode;
    std::strong_ordering operator<=>(const Keysym& other) const = default;
};

// clang-format off
export inline const std::array<std::string, 47> literal_keycode_str =
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

export inline constexpr std::array<uint32_t, 47> literal_keycode_value = {
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

export int getImplicitFlags(const std::string& literal);

// Enum for known literal keys
export enum class LiteralKey : uint32_t {
    Return = 0,
    Tab,
    Space,
    Backspace,
    Escape,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Left,
    Right,
    Up,
    Down,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    SoundUp,
    SoundDown,
    Mute,
    Play,
    Previous,
    Next,
    Rewind,
    Fast,
    BrightnessUp,
    BrightnessDown,
    IlluminationUp,
    IlluminationDown
};

export inline std::string literalKeyToString(LiteralKey k) {
    return literal_keycode_str[static_cast<size_t>(k)];
}

export inline uint32_t literalKeyToKeycode(LiteralKey k) {
    return literal_keycode_value[static_cast<size_t>(k)];
}

export inline std::optional<LiteralKey> tryParseLiteralKey(const std::string& name) {
    for (size_t i = 0; i < literal_keycode_str.size(); i++) {
        if (literal_keycode_str[i] == name) return static_cast<LiteralKey>(i);
    }
    return std::nullopt;
}

// Offsets and flags are provided by smhkd.modifier

export inline int getImplicitFlags(const std::string& literal) {
    constexpr int KEY_HAS_IMPLICIT_FN_MOD = 4;
    constexpr int KEY_HAS_IMPLICIT_NX_MOD = 35;
    const auto* it = std::ranges::find(literal_keycode_str, literal);
    auto literal_index = std::distance(literal_keycode_str.begin(), it);

    int flags{};
    if ((literal_index > KEY_HAS_IMPLICIT_FN_MOD) && (literal_index < KEY_HAS_IMPLICIT_NX_MOD)) {
        flags = hotkey_flags[FN_MOD_OFFSET];
    } else if (literal_index >= KEY_HAS_IMPLICIT_NX_MOD) {
        flags = hotkey_flags[NX_MOD_OFFSET];
    }
    return flags;
}
