#include "keysym.hpp"

#include <algorithm>

#include "modifier.hpp"

constexpr int KEY_HAS_IMPLICIT_FN_MOD = 4;
constexpr int KEY_HAS_IMPLICIT_NX_MOD = 35;

int getImplicitFlags(const std::string& literal) {
    const auto* it = std::ranges::find(literal_keycode_str, literal);
    auto literal_index = std::distance(literal_keycode_str.begin(), it);

    int flags{};
    if ((literal_index > KEY_HAS_IMPLICIT_FN_MOD) && (literal_index < KEY_HAS_IMPLICIT_NX_MOD)) {
        flags = hotkey_flags[FN_MOD_OFFSET];
    } else if (literal_index >= KEY_HAS_IMPLICIT_NX_MOD) {
        flags = hotkey_flags[NX_MOD_OFFSET];
    }
    return flags;
}
