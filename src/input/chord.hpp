#pragma once

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <compare>
#include <format>

#include "keysym.hpp"
#include "modifier.hpp"

struct Chord {
    Keysym keysym;
    ModifierFlags modifiers;

    std::strong_ordering operator<=>(const Chord& other) const = default;

    [[nodiscard]] bool isActivatedBy(const Chord& eventInput) const;
};

template <>
struct std::formatter<Chord> : std::formatter<std::string_view> {
    auto format(const Chord& c, std::format_context& ctx) const {
        if (c.modifiers.flags == 0) {
            return std::format_to(ctx.out(), "{}", c.keysym);
        }

        return std::format_to(ctx.out(), "{} + {}", c.modifiers, c.keysym);
    }
};
