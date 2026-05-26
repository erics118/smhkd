#include "modifier.hpp"

int builtinModifierToFlags(BuiltinModifier m) {
    return builtin_modifiers[static_cast<size_t>(m)].flag;
}

std::optional<BuiltinModifier> parseBuiltinModifier(const std::string& name) {
    for (size_t i = 0; i < builtin_modifiers.size(); i++) {
        if (builtin_modifiers[i].name == name) return static_cast<BuiltinModifier>(i);
    }
    return std::nullopt;
}

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
