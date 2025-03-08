#include "locale.hpp"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>

#include "keysym.hpp"
#include "log.hpp"
#include "utils.hpp"

bool initializeKeycodeMap() {
    static const std::array<uint32_t, 36> layoutDependentKeycodes = {
        kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E,
        kVK_ANSI_F, kVK_ANSI_G, kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J,
        kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N, kVK_ANSI_O,
        kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T,
        kVK_ANSI_U, kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y,
        kVK_ANSI_Z, kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3,
        kVK_ANSI_4, kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8,
        kVK_ANSI_9};

    UniChar chars[255];
    UniCharCount len{};
    UInt32 state{};

    std::unique_ptr<std::remove_pointer_t<TISInputSourceRef>, decltype(&CFRelease)>
        keyboard(TISCopyCurrentASCIICapableKeyboardLayoutInputSource(), CFRelease);

    if (!keyboard) return false;

    const auto* uchr = static_cast<CFDataRef>(TISGetInputSourceProperty(keyboard.get(),
        kTISPropertyUnicodeKeyLayoutData));
    if (!uchr) return false;

    const auto* keyboardLayout = reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(uchr));
    if (!keyboardLayout) return false;

    keycodeMap.clear();

    for (uint32_t keycode : layoutDependentKeycodes) {
        if (UCKeyTranslate(keyboardLayout,
                keycode,
                kUCKeyActionDown,
                0,
                LMGetKbdType(),
                kUCKeyTranslateNoDeadKeysMask,
                &state,
                std::size(chars),
                &len,
                chars)
                == noErr
            && len > 0) {
            CFStringRef keyCfString = CFStringCreateWithCharacters(nullptr, chars, len);
            if (!keyCfString) continue;

            std::string keyString = cfStringToString(keyCfString);
            CFRelease(keyCfString);

            if (!keyString.empty()) {
                keycodeMap[keyString] = keycode;
            }
        }
    }

    return !keycodeMap.empty();
}

uint32_t getKeycode(const std::string& key) {
    // debug("getting keycode for '{}'", key);

    if (key.length() == 1) {
        auto it = keycodeMap.find(key);
        if (it != keycodeMap.end()) {
            return static_cast<uint32_t>(it->second);
        }
    }

    // handle literals, ie enter, space, tab, etc
    const auto* it = std::ranges::find(literal_keycode_str, key);
    if (it != literal_keycode_str.end()) {
        return literal_keycode_value[std::distance(literal_keycode_str.begin(), it)];
    }

    try {
        return std::stoi(key, nullptr, 16);
    } catch (...) {
        error("Keycode not found for '{}'", key);
    }

    return -1;
}

std::string getNameOfKeycode(uint32_t keycode) {
    for (const auto& [key, code] : keycodeMap) {
        if (code == keycode) {
            return key;
        }
    }

    for (int i = 0; i < literal_keycode_str.size(); i++) {
        if (literal_keycode_value[i] == keycode) {
            return literal_keycode_str[i];
        }
    }

    // return as hex
    return std::format("0x{:x}", keycode);
}
