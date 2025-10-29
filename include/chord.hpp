#pragma once

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include "keysym.hpp"
#include "modifier.hpp"
#include "token.hpp"

struct Chord {
    Keysym keysym;
    ModifierFlags modifiers;

    std::strong_ordering operator<=>(const Chord& other) const = default;

    void setKeycode(const Token& t);

    // check if this chord is activated by another chord
    // usage: chord.isActivatedBy(event_input_chord)
    [[nodiscard]] bool isActivatedBy(const Chord& other) const;
};

template <>
struct std::formatter<Chord> : std::formatter<std::string_view> {
    auto format(const Chord& chord, std::format_context& ctx) const {
        if (chord.modifiers.flags == 0) {
            return std::format_to(ctx.out(), "{}", chord.keysym);
        }
        return std::format_to(ctx.out(), "{} + {}", chord.modifiers, chord.keysym);
    }
};
