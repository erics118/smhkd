
#include "utils.hpp"

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
