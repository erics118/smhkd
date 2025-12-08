module;

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <compare>
#include <format>

export module chord;

import token;
import keysym;
import modifier;

export struct Chord {
    Keysym keysym;
    ModifierFlags modifiers;

    std::strong_ordering operator<=>(const Chord& other) const = default;

    [[nodiscard]] bool isActivatedBy(const Chord& eventInput) const;
};

export template <>
struct std::formatter<Chord> : std::formatter<std::string_view> {
    auto format(const Chord& c, std::format_context& ctx) const {
        std::string modStr = std::format("{}", c.modifiers);
        std::string keyStr = std::format("{}", c.keysym);

        if (modStr.empty()) {
            return std::format_to(ctx.out(), "{}", keyStr);
        }

        return std::format_to(ctx.out(), "{}+{}", modStr, keyStr);
    }
};

bool Chord::isActivatedBy(const Chord& eventInput) const {
    return modifiers.isActivatedBy(eventInput.modifiers) && this->keysym == eventInput.keysym;
}
