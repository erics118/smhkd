module;

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <array>
#include <format>
#include <optional>
#include <string>

export module smhkd.modifier;

export enum HotkeyFlag {
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

export constexpr int l_offset = 1;
export constexpr int r_offset = 2;

export constexpr std::array<int, 13> cgevent_flags = {
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

export constexpr std::array<HotkeyFlag, 13> hotkey_flags = {
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

export const std::array<std::string, 13> hotkey_flag_names = {
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

export enum class BuiltinModifier {
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

export int builtinModifierToFlags(BuiltinModifier m) {
    return hotkey_flags[static_cast<size_t>(m)];
}

export std::string builtinModifierToString(BuiltinModifier m) {
    return hotkey_flag_names[static_cast<size_t>(m)];
}

export std::optional<BuiltinModifier> parseBuiltinModifier(const std::string& name) {
    for (size_t i = 0; i < hotkey_flag_names.size(); i++) {
        if (hotkey_flag_names[i] == name) return static_cast<BuiltinModifier>(i);
    }
    return std::nullopt;
}

export constexpr int ALT_MOD_OFFSET = 0;
export constexpr int SHIFT_MOD_OFFSET = 3;
export constexpr int CMD_MOD_OFFSET = 6;
export constexpr int CTRL_MOD_OFFSET = 9;
export constexpr int FN_MOD_OFFSET = 12;
export constexpr int NX_MOD_OFFSET = 13;

export struct ModifierFlags {
    int flags;
    std::strong_ordering operator<=>(const ModifierFlags& other) const = default;
    [[nodiscard]] bool isActivatedBy(const ModifierFlags& other) const;
    [[nodiscard]] bool has(uint32_t flag) const;
};

namespace {
int eventLRModifierFlagsToHotkeyFlags(CGEventFlags eventflags, int mod) {
    int flags{};
    int mask = cgevent_flags[mod];
    int lmask = cgevent_flags[mod + l_offset];
    int rmask = cgevent_flags[mod + r_offset];
    if ((eventflags & mask) == mask) {
        bool left = (eventflags & lmask) == lmask;
        bool right = (eventflags & rmask) == rmask;
        if (left) flags |= hotkey_flags[mod + l_offset];
        if (right) flags |= hotkey_flags[mod + r_offset];
        if (!left && !right) flags |= hotkey_flags[mod];
    }
    return flags;
}

bool compareLRModifier(const ModifierFlags& a, const ModifierFlags& b, int mod) {
    if (a.has(hotkey_flags[mod])) {
        return b.has(hotkey_flags[mod + l_offset]) || b.has(hotkey_flags[mod + r_offset]) || b.has(hotkey_flags[mod]);
    }
    return a.has(hotkey_flags[mod + l_offset]) == b.has(hotkey_flags[mod + l_offset])
        && a.has(hotkey_flags[mod + r_offset]) == b.has(hotkey_flags[mod + r_offset])
        && a.has(hotkey_flags[mod]) == b.has(hotkey_flags[mod]);
}
}  // namespace

bool ModifierFlags::has(const uint32_t flag) const { return flags & flag; }

export ModifierFlags eventModifierFlagsToHotkeyFlags(CGEventFlags flags) {
    int res{};
    res |= eventLRModifierFlagsToHotkeyFlags(flags, ALT_MOD_OFFSET);
    res |= eventLRModifierFlagsToHotkeyFlags(flags, SHIFT_MOD_OFFSET);
    res |= eventLRModifierFlagsToHotkeyFlags(flags, CMD_MOD_OFFSET);
    res |= eventLRModifierFlagsToHotkeyFlags(flags, CTRL_MOD_OFFSET);
    if ((flags & cgevent_flags[FN_MOD_OFFSET]) == cgevent_flags[FN_MOD_OFFSET]) {
        res |= hotkey_flags[FN_MOD_OFFSET];
    }
    return ModifierFlags{res};
}

bool ModifierFlags::isActivatedBy(const ModifierFlags& other) const {
    return compareLRModifier(*this, other, ALT_MOD_OFFSET)
        && compareLRModifier(*this, other, SHIFT_MOD_OFFSET)
        && compareLRModifier(*this, other, CMD_MOD_OFFSET)
        && compareLRModifier(*this, other, CTRL_MOD_OFFSET)
        && has(hotkey_flags[FN_MOD_OFFSET]) == other.has(hotkey_flags[FN_MOD_OFFSET])
        && has(hotkey_flags[NX_MOD_OFFSET]) == other.has(hotkey_flags[NX_MOD_OFFSET]);
}
