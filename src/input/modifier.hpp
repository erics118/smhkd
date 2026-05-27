#pragma once

#include <CoreGraphics/CGEventTypes.h>

#include <array>
#include <format>
#include <optional>
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

enum class BuiltinModifier {
    Alt = 0,
    LAlt,
    RAlt,
    Shift,
    LShift,
    RShift,
    Cmd,
    LCmd,
    RCmd,
    Ctrl,
    LCtrl,
    RCtrl,
    Fn,
};

struct ModifierEntry {
    const char* name;
    HotkeyFlag flag;
};

// clang-format off
constexpr std::array<ModifierEntry, 13> builtin_modifiers = {{
    {"alt",    Hotkey_Flag_Alt},
    {"lalt",   Hotkey_Flag_LAlt},
    {"ralt",   Hotkey_Flag_RAlt},
    {"shift",  Hotkey_Flag_Shift},
    {"lshift", Hotkey_Flag_LShift},
    {"rshift", Hotkey_Flag_RShift},
    {"cmd",    Hotkey_Flag_Cmd},
    {"lcmd",   Hotkey_Flag_LCmd},
    {"rcmd",   Hotkey_Flag_RCmd},
    {"ctrl",   Hotkey_Flag_Control},
    {"lctrl",  Hotkey_Flag_LControl},
    {"rctrl",  Hotkey_Flag_RControl},
    {"fn",     Hotkey_Flag_Fn},
}};

struct LRModifierGroup {
    HotkeyFlag generic;
    HotkeyFlag left;
    HotkeyFlag right;
    int cgevent_generic;
    int cgevent_left;
    int cgevent_right;
};

constexpr std::array<LRModifierGroup, 4> lr_modifier_groups = {{
    {Hotkey_Flag_Alt,     Hotkey_Flag_LAlt,     Hotkey_Flag_RAlt,     kCGEventFlagMaskAlternate, NX_DEVICELALTKEYMASK,   NX_DEVICERALTKEYMASK  },
    {Hotkey_Flag_Shift,   Hotkey_Flag_LShift,   Hotkey_Flag_RShift,   kCGEventFlagMaskShift,     NX_DEVICELSHIFTKEYMASK, NX_DEVICERSHIFTKEYMASK},
    {Hotkey_Flag_Cmd,     Hotkey_Flag_LCmd,     Hotkey_Flag_RCmd,     kCGEventFlagMaskCommand,   NX_DEVICELCMDKEYMASK,   NX_DEVICERCMDKEYMASK  },
    {Hotkey_Flag_Control, Hotkey_Flag_LControl, Hotkey_Flag_RControl, kCGEventFlagMaskControl,   NX_DEVICELCTLKEYMASK,   NX_DEVICERCTLKEYMASK  },
}};
// clang-format on

int builtinModifierToFlags(BuiltinModifier m);

template <>
struct std::formatter<BuiltinModifier> : std::formatter<std::string_view> {
    auto format(const BuiltinModifier& m, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", builtin_modifiers[static_cast<size_t>(m)].name);
    }
};

std::optional<BuiltinModifier> parseBuiltinModifier(const std::string& name);

struct ModifierFlags {
    int flags;

    std::strong_ordering operator<=>(const ModifierFlags& other) const = default;

    [[nodiscard]] bool isActivatedBy(const ModifierFlags& other) const;

    [[nodiscard]] bool has(uint32_t flag) const;
};

template <>
struct std::formatter<ModifierFlags> : std::formatter<std::string_view> {
    auto format(const ModifierFlags& m, std::format_context& ctx) const {
        if (m.flags == 0) return std::format_to(ctx.out(), "");

        auto out = ctx.out();
        bool first = true;
        for (const auto& entry : builtin_modifiers) {
            if (!m.has(entry.flag)) continue;
            if (!first) out = std::format_to(out, " + ");
            out = std::format_to(out, "{}", entry.name);
            first = false;
        }
        if (m.has(Hotkey_Flag_NX)) {
            if (!first) out = std::format_to(out, " + ");
            out = std::format_to(out, "nx");
            first = false;
        }

        if (first) return std::format_to(ctx.out(), "");
        return out;
    }
};

int eventFlagsToHotkeyFlags(CGEventFlags eventFlags, const LRModifierGroup& group);

ModifierFlags eventModifierFlagsToHotkeyFlags(CGEventFlags flags);

CGEventFlags hotkeyFlagsToEventFlags(ModifierFlags flags);
