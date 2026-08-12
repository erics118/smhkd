#pragma once

#include <compare>
#include <format>
#include <optional>

#include "keysym.hpp"
#include "modifier.hpp"

struct Chord {
    Keysym keysym;
    ModifierFlags modifiers;
    // required fingers on the trackpad, or unset for no requirement
    std::optional<int> fingerCount;

    std::strong_ordering operator<=>(const Chord& other) const = default;

    [[nodiscard]] bool isActivatedBy(const Chord& eventInput, int liveFingerCount) const;
};

template <>
struct std::formatter<Chord> : std::formatter<std::string_view> {
    auto format(const Chord& c, std::format_context& ctx) const {
        auto out = ctx.out();
        if (c.fingerCount) {
            out = std::format_to(out, "trackpad_fingers({}) + ", *c.fingerCount);
        }
        if (c.modifiers.flags == 0) {
            return std::format_to(out, "{}", c.keysym);
        }
        return std::format_to(out, "{} + {}", c.modifiers, c.keysym);
    }
};
