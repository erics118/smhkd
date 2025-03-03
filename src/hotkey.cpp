#include "hotkey.hpp"

#include "utils.hpp"

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
bool compareLRModifier(const Hotkey& a, const Hotkey& b, int mod) {
    // Check if generic modifier is set in hotkey a
    if (has_flags(a, hotkey_flags[mod])) {
        // If generic modifier is set in a, then b must have either:
        // - left modifier, or
        // - right modifier, or
        // - generic modifier
        return has_flags(b, hotkey_flags[mod + l_offset])
            || has_flags(b, hotkey_flags[mod + r_offset])
            || has_flags(b, hotkey_flags[mod]);
    }

    // If generic modifier is not set in a, then specific modifiers must match exactly
    return has_flags(a, hotkey_flags[mod + l_offset]) == has_flags(b, hotkey_flags[mod + l_offset])
        && has_flags(a, hotkey_flags[mod + r_offset]) == has_flags(b, hotkey_flags[mod + r_offset])
        && has_flags(a, hotkey_flags[mod]) == has_flags(b, hotkey_flags[mod]);
}

bool compareFn(const Hotkey& a, const Hotkey& b) {
    return has_flags(a, hotkey_flags[fn_mod_offset]) == has_flags(b, hotkey_flags[fn_mod_offset]);
}

bool compareNX(const Hotkey& a, const Hotkey& b) {
    return has_flags(a, hotkey_flags[nx_mod_offset]) == has_flags(b, hotkey_flags[nx_mod_offset]);
}

}  // namespace

int eventModifierFlagsToHotkeyFlags(CGEventFlags flags) {
    int res{};

    res |= eventLRModifierFlagsToHotkeyFlags(flags, alt_mod_offset);
    res |= eventLRModifierFlagsToHotkeyFlags(flags, shift_mod_offset);
    res |= eventLRModifierFlagsToHotkeyFlags(flags, cmd_mod_offset);
    res |= eventLRModifierFlagsToHotkeyFlags(flags, ctrl_mod_offset);

    if ((flags & cgevent_flags[fn_mod_offset]) == cgevent_flags[fn_mod_offset]) {
        res |= hotkey_flags[fn_mod_offset];
    }
    return res;
}

bool Hotkey::operator==(const Hotkey& other) const {
    return compareLRModifier(*this, other, alt_mod_offset)
        && compareLRModifier(*this, other, shift_mod_offset)
        && compareLRModifier(*this, other, cmd_mod_offset)
        && compareLRModifier(*this, other, ctrl_mod_offset)
        && compareFn(*this, other)
        && compareNX(*this, other)
        && this->keyCode == other.keyCode;
}
