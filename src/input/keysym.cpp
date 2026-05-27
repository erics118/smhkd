#include "keysym.hpp"

#include "../input/modifier.hpp"

uint32_t literalKeyToKeycode(LiteralKey k) {
    return literal_keys[static_cast<size_t>(k)].keycode;
}

std::optional<LiteralKey> parseLiteralKey(const std::string& name) {
    for (size_t i = 0; i < literal_keys.size(); i++) {
        if (literal_keys[i].name == name) return static_cast<LiteralKey>(i);
    }
    return std::nullopt;
}

int getImplicitFlags(LiteralKey k) {
    // Indices 5-34 (delete through f20) need fn: forward-delete, navigation, and function keys.
    // Indices 35+ (media keys) need nx.
    constexpr size_t FIRST_FN_KEY_INDEX = 5;
    constexpr size_t FIRST_NX_KEY_INDEX = 35;

    auto i = static_cast<size_t>(k);
    if (i >= FIRST_FN_KEY_INDEX && i < FIRST_NX_KEY_INDEX) return Hotkey_Flag_Fn;
    if (i >= FIRST_NX_KEY_INDEX) return Hotkey_Flag_NX;
    return 0;
}

uint32_t getKeycode(char key) {
    if (auto keycode = lookupKeycode(std::string_view(&key, 1))) {
        return *keycode;
    }

    return static_cast<uint32_t>(static_cast<unsigned char>(key));
}
