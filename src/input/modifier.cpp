#include "modifier.hpp"
namespace {

int eventLrFlagsToHotkeyFlags(CGEventFlags eventFlags, const LRModifierGroup& group) {
    int flags{};
    if ((eventFlags & group.cgevent_generic) == group.cgevent_generic) {
        bool left = (eventFlags & group.cgevent_left) == group.cgevent_left;
        bool right = (eventFlags & group.cgevent_right) == group.cgevent_right;
        if (left) flags |= group.left;
        if (right) flags |= group.right;
        if (!left && !right) flags |= group.generic;
    }
    return flags;
}

}  // namespace

int builtinModifierToFlags(BuiltinModifier m) {
    return builtin_modifiers[static_cast<size_t>(m)].flag;
}

std::optional<BuiltinModifier> parseBuiltinModifier(const std::string& name) {
    for (size_t i = 0; i < builtin_modifiers.size(); i++) {
        if (builtin_modifiers[i].name == name) return static_cast<BuiltinModifier>(i);
    }
    return std::nullopt;
}

bool ModifierFlags::has(const uint32_t flag) const { return flags & flag; }

ModifierFlags eventModifierFlagsToHotkeyFlags(CGEventFlags flags) {
    int res{};
    for (const auto& group : lr_modifier_groups) {
        res |= eventLrFlagsToHotkeyFlags(flags, group);
    }
    if ((flags & kCGEventFlagMaskSecondaryFn) == kCGEventFlagMaskSecondaryFn) {
        res |= Hotkey_Flag_Fn;
    }
    return ModifierFlags{res};
}

CGEventFlags hotkeyFlagsToEventFlags(ModifierFlags flags) {
    CGEventFlags eventFlags = 0;
    for (const auto& group : lr_modifier_groups) {
        if (flags.has(group.left) || flags.has(group.right) || flags.has(group.generic)) {
            eventFlags |= static_cast<CGEventFlags>(group.cgevent_generic);
        }
        if (flags.has(group.left)) {
            eventFlags |= static_cast<CGEventFlags>(group.cgevent_left);
        }
        if (flags.has(group.right)) {
            eventFlags |= static_cast<CGEventFlags>(group.cgevent_right);
        }
    }
    if (flags.has(Hotkey_Flag_Fn)) {
        eventFlags |= kCGEventFlagMaskSecondaryFn;
    }
    return eventFlags;
}

bool ModifierFlags::isActivatedBy(const ModifierFlags& other) const {
    for (const auto& group : lr_modifier_groups) {
        // if this has generic, other must have one of generic/left/right
        if (has(group.generic)) {
            if (!other.has(group.left) && !other.has(group.right) && !other.has(group.generic)) {
                return false;
            }
        } else {
            // otherwise, left/right must match exactly
            if (has(group.left) != other.has(group.left) || has(group.right) != other.has(group.right)) {
                return false;
            }
        }
    }
    return has(Hotkey_Flag_Fn) == other.has(Hotkey_Flag_Fn)
        && has(Hotkey_Flag_NX) == other.has(Hotkey_Flag_NX);
}
