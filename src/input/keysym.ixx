module;

#include <Carbon/Carbon.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <algorithm>
#include <array>
#include <compare>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <unordered_map>

export module keysym;

import modifier;
import locale;

export struct Keysym {
    uint32_t keycode;
    std::strong_ordering operator<=>(const Keysym& other) const = default;
};

struct LiteralKeyInfo {
    const char* name;
    uint32_t keycode;
};

// clang-format off
export constexpr std::array<LiteralKeyInfo, 47> literal_keys = {{
    {"return",            kVK_Return                 },
    {"tab",               kVK_Tab                    },
    {"space",             kVK_Space                  },
    {"backspace",         kVK_Delete                 },
    {"escape",            kVK_Escape                 },
    {"delete",            kVK_ForwardDelete          },
    {"home",              kVK_Home                   },
    {"end",               kVK_End                    },
    {"pageup",            kVK_PageUp                 },
    {"pagedown",          kVK_PageDown               },
    {"insert",            kVK_Help                   },
    {"left",              kVK_LeftArrow              },
    {"right",             kVK_RightArrow             },
    {"up",                kVK_UpArrow                },
    {"down",              kVK_DownArrow              },
    {"f1",                kVK_F1                     },
    {"f2",                kVK_F2                     },
    {"f3",                kVK_F3                     },
    {"f4",                kVK_F4                     },
    {"f5",                kVK_F5                     },
    {"f6",                kVK_F6                     },
    {"f7",                kVK_F7                     },
    {"f8",                kVK_F8                     },
    {"f9",                kVK_F9                     },
    {"f10",               kVK_F10                    },
    {"f11",               kVK_F11                    },
    {"f12",               kVK_F12                    },
    {"f13",               kVK_F13                    },
    {"f14",               kVK_F14                    },
    {"f15",               kVK_F15                    },
    {"f16",               kVK_F16                    },
    {"f17",               kVK_F17                    },
    {"f18",               kVK_F18                    },
    {"f19",               kVK_F19                    },
    {"f20",               kVK_F20                    },
    {"sound_up",          NX_KEYTYPE_SOUND_UP        },
    {"sound_down",        NX_KEYTYPE_SOUND_DOWN      },
    {"mute",              NX_KEYTYPE_MUTE            },
    {"play",              NX_KEYTYPE_PLAY            },
    {"previous",          NX_KEYTYPE_PREVIOUS        },
    {"next",              NX_KEYTYPE_NEXT            },
    {"rewind",            NX_KEYTYPE_REWIND          },
    {"fast",              NX_KEYTYPE_FAST            },
    {"brightness_up",     NX_KEYTYPE_BRIGHTNESS_UP   },
    {"brightness_down",   NX_KEYTYPE_BRIGHTNESS_DOWN },
    {"illumination_up",   NX_KEYTYPE_ILLUMINATION_UP },
    {"illumination_down", NX_KEYTYPE_ILLUMINATION_DOWN},
}};


// Enum for known literal keys
export enum class LiteralKey : uint32_t {
    Return = 0,     Tab,            Space,
    Backspace,      Escape,         Delete,
    Home,           End,            PageUp,
    PageDown,       Insert,         Left,
    Right,          Up,             Down,
    F1,             F2,             F3,
    F4,             F5,             F6,
    F7,             F8,             F9,
    F10,            F11,            F12,
    F13,            F14,            F15,
    F16,            F17,            F18,
    F19,            F20,

    SoundUp,        SoundDown,      Mute,
    Play,           Previous,       Next,
    Rewind,         Fast,           BrightnessUp,
    BrightnessDown, IlluminationUp, IlluminationDown
};


export template <>
struct std::formatter<LiteralKey> : std::formatter<std::string_view> {
    auto format(const LiteralKey& k, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", literal_keys[static_cast<size_t>(k)].name);
    }
};
// clang-format on

export uint32_t literalKeyToKeycode(LiteralKey k) {
    return literal_keys[static_cast<size_t>(k)].keycode;
}

export std::optional<LiteralKey> tryParseLiteralKey(const std::string& name) {
    for (size_t i = 0; i < literal_keys.size(); i++) {
        if (literal_keys[i].name == name) return static_cast<LiteralKey>(i);
    }
    return std::nullopt;
}

export int getImplicitFlags(LiteralKey k) {
    // Indices 5–34 (delete through f20) need fn: forward-delete, navigation, and function keys.
    // Indices 35+ (media keys) need nx.
    constexpr size_t FIRST_FN_KEY_INDEX = 5;
    constexpr size_t FIRST_NX_KEY_INDEX = 35;

    auto i = static_cast<size_t>(k);
    if (i >= FIRST_FN_KEY_INDEX && i < FIRST_NX_KEY_INDEX) return Hotkey_Flag_Fn;
    if (i >= FIRST_NX_KEY_INDEX) return Hotkey_Flag_NX;
    return 0;
}

// convert a single character to a keycode
export uint32_t getKeycode(char key) {
    // check the locale-dependent keycode map first
    if (const auto it = keycodeMap.find(std::string{key}); it != keycodeMap.end()) {
        return it->second;
    }

    // fallback to use char value directly
    return static_cast<uint32_t>(static_cast<unsigned char>(key));
}

export template <>
struct std::formatter<Keysym> : std::formatter<std::string_view> {
    auto format(const Keysym& k, std::format_context& ctx) const {
        for (const auto& [key, code] : keycodeMap) {
            if (code == k.keycode) {
                return std::format_to(ctx.out(), "{}", key);
            }
        }
        for (const auto& entry : literal_keys) {
            if (entry.keycode == k.keycode) {
                return std::format_to(ctx.out(), "{}", entry.name);
            }
        }
        return std::format_to(ctx.out(), "{:#02x}", k.keycode);
    }
};
