#include "locale.hpp"

#include <array>
#include <unordered_map>

#include "../common/cf_string.hpp"

namespace {

std::unordered_map<std::string, Keycode> buildKeycodeMap() {
    std::unordered_map<std::string, Keycode> keycodeMap;

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

    if (!keyboard) return {};

    const auto* uchr = static_cast<CFDataRef>(TISGetInputSourceProperty(keyboard.get(),
        kTISPropertyUnicodeKeyLayoutData));
    if (!uchr) return {};

    const auto* keyboardLayout = reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(uchr));
    if (!keyboardLayout) return {};

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

    return keycodeMap;
}

const std::unordered_map<std::string, Keycode>& keycodeMap() {
    static const auto map = buildKeycodeMap();
    return map;
}

}  // namespace

bool initializeKeycodeMap() {
    return !keycodeMap().empty();
}

std::optional<Keycode> lookupKeycode(std::string_view key) {
    if (const auto it = keycodeMap().find(std::string(key)); it != keycodeMap().end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::string> lookupKeyString(Keycode keycode) {
    for (const auto& [key, code] : keycodeMap()) {
        if (code == keycode) {
            return key;
        }
    }
    return std::nullopt;
}
