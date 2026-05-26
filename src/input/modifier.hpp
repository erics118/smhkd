#pragma once

#include <CoreGraphics/CGEventTypes.h>

#include <array>
#include <format>
#include <optional>
#include <string>
#include <vector>


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

int builtinModifierToFlags(BuiltinModifier m) {
    return builtin_modifiers[static_cast<size_t>(m)].flag;
}

template <>
struct std::formatter<BuiltinModifier> : std::formatter<std::string_view> {
    auto format(const BuiltinModifier& m, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", builtin_modifiers[static_cast<size_t>(m)].name);
    }
};

std::optional<BuiltinModifier> parseBuiltinModifier(const std::string& name) {
    for (size_t i = 0; i < builtin_modifiers.size(); i++) {
        if (builtin_modifiers[i].name == name) return static_cast<BuiltinModifier>(i);
    }
    return std::nullopt;
}

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

        std::vector<std::string> parts;
        for (const auto& entry : builtin_modifiers) {
            if (m.has(entry.flag)) parts.push_back(entry.name);
        }
        if (m.has(Hotkey_Flag_NX)) parts.push_back("nx");

        if (parts.empty()) return std::format_to(ctx.out(), "");

        std::string result = parts[0];
        for (size_t i = 1; i < parts.size(); ++i) result += " + " + parts[i];
        return std::format_to(ctx.out(), "{}", result);
    }
};

int eventLRModifierFlagsToHotkeyFlags(CGEventFlags eventflags, const LRModifierGroup& group) {
    int flags{};
    if ((eventflags & group.cgevent_generic) == group.cgevent_generic) {
        bool left = (eventflags & group.cgevent_left) == group.cgevent_left;
        bool right = (eventflags & group.cgevent_right) == group.cgevent_right;
        if (left) flags |= group.left;
        if (right) flags |= group.right;
        if (!left && !right) flags |= group.generic;
    }
    return flags;
}

bool compareLRModifier(const ModifierFlags& a, const ModifierFlags& b, const LRModifierGroup& group) {
    if (a.has(group.generic)) {
        return b.has(group.left) || b.has(group.right) || b.has(group.generic);
    }
    return a.has(group.left) == b.has(group.left)
        && a.has(group.right) == b.has(group.right)
        && a.has(group.generic) == b.has(group.generic);
}

bool ModifierFlags::has(const uint32_t flag) const { return flags & flag; }

ModifierFlags eventModifierFlagsToHotkeyFlags(CGEventFlags flags) {
    int res{};
    for (const auto& group : lr_modifier_groups) {
        res |= eventLRModifierFlagsToHotkeyFlags(flags, group);
    }
    if ((flags & kCGEventFlagMaskSecondaryFn) == kCGEventFlagMaskSecondaryFn) {
        res |= Hotkey_Flag_Fn;
    }
    return ModifierFlags{res};
}

bool ModifierFlags::isActivatedBy(const ModifierFlags& other) const {
    for (const auto& group : lr_modifier_groups) {
        if (!compareLRModifier(*this, other, group)) return false;
    }
    return has(Hotkey_Flag_Fn) == other.has(Hotkey_Flag_Fn)
        && has(Hotkey_Flag_NX) == other.has(Hotkey_Flag_NX);
}
