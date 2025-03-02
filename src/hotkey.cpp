#include "hotkey.hpp"

#include "utils.hpp"

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int cgevent_lrmod_flag_to_hotkey_flags(CGEventFlags eventflags, int mod) {
    int flags = 0;

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

int convert_cgevent_flags_to_hotkey_flags(CGEventFlags flags) {
    int res = 0;

    res |= cgevent_lrmod_flag_to_hotkey_flags(flags, alt_mod);
    res |= cgevent_lrmod_flag_to_hotkey_flags(flags, shift_mod);
    res |= cgevent_lrmod_flag_to_hotkey_flags(flags, cmd_mod);
    res |= cgevent_lrmod_flag_to_hotkey_flags(flags, ctrl_mod);

    if ((flags & cgevent_flags[fn_mod]) == cgevent_flags[fn_mod]) {
        res |= hotkey_flags[fn_mod];
    }
    return res;
}

// first hotkey should be the config, second hotkey should be the event
bool compare_lr_mod(const Hotkey& a, const Hotkey& b, int mod) {
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

bool compare_fn(const Hotkey& a, const Hotkey& b) {
    return has_flags(a, hotkey_flags[fn_mod]) == has_flags(b, hotkey_flags[fn_mod]);
}

bool compare_key(const Hotkey& a, const Hotkey& b) {
    return a.keyCode == b.keyCode;
}

// first hotkey should be the config, second hotkey should be the event
bool same_hotkey(const Hotkey& a, const Hotkey& b) {
    return compare_lr_mod(a, b, alt_mod)
        && compare_lr_mod(a, b, shift_mod)
        && compare_lr_mod(a, b, cmd_mod)
        && compare_lr_mod(a, b, ctrl_mod)
        && compare_fn(a, b)
        && compare_key(a, b);
}
