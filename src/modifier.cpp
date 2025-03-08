#include "modifier.hpp"

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <array>
#include <print>

namespace {

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
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

// first hotkey should be the config, second hotkey should be the event
bool compareLRModifier(const ModifierFlags& a, const ModifierFlags& b, int mod) {
    // Check if generic modifier is set in hotkey a
    if (a.has(hotkey_flags[mod])) {
        // If generic modifier is set in a, then b must have either:
        // - left modifier, or
        // - right modifier, or
        // - generic modifier
        return a.has(hotkey_flags[mod + l_offset])
            || b.has(hotkey_flags[mod + r_offset])
            || b.has(hotkey_flags[mod]);
    }

    // If generic modifier is not set in a, then specific modifiers must match exactly
    return a.has(hotkey_flags[mod + l_offset]) == b.has(hotkey_flags[mod + l_offset])
        && a.has(hotkey_flags[mod + r_offset]) == b.has(hotkey_flags[mod + r_offset])
        && a.has(hotkey_flags[mod]) == b.has(hotkey_flags[mod]);
}

bool compareFn(const ModifierFlags& a, const ModifierFlags& b) {
    return a.has(hotkey_flags[FN_MOD_OFFSET]) == b.has(hotkey_flags[FN_MOD_OFFSET]);
}

bool compareNX(const ModifierFlags& a, const ModifierFlags& b) {
    return a.has(hotkey_flags[NX_MOD_OFFSET]) == b.has(hotkey_flags[NX_MOD_OFFSET]);
}

}  // namespace

bool ModifierFlags::has(uint32_t flag) const {
    bool result = flags & flag;
    return result;
}

ModifierFlags eventModifierFlagsToHotkeyFlags(CGEventFlags flags) {
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
        && compareFn(*this, other)
        && compareNX(*this, other);
}
