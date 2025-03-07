#include "keysym.hpp"

const int key_has_implicit_fn_mod = 4;
const int key_has_implicit_nx_mod = 35;

int getImplicitFlags(const std::string& literal) {
    const auto* it = std::ranges::find(literal_keycode_str, literal);
    auto literal_index = std::distance(literal_keycode_str.begin(), it);

    int flags{};
    if ((literal_index > key_has_implicit_fn_mod) && (literal_index < key_has_implicit_nx_mod)) {
        flags = Hotkey_Flag_Fn;
    } else if (literal_index >= key_has_implicit_nx_mod) {
        flags = Hotkey_Flag_NX;
    }
    return flags;
}
