#include "keysym.hpp"

#include "hotkey.hpp"
#include "locale.hpp"

std::string getNameOfKeysym(Keysym keysym) {
    for (const auto& [key, code] : keycodeMap) {
        if (code == keysym.keycode) {
            return key;
        }
    }

    for (int i = 0; i < literal_keycode_str.size(); i++) {
        if (literal_keycode_value[i] == keysym.keycode) {
            return literal_keycode_str[i];
        }
    }

    // return as hex
    return std::format("0x{:x}", keysym.keycode);
}
