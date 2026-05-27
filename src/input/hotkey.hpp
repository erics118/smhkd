#pragma once

#include <CoreGraphics/CGEventTypes.h>

#include <compare>
#include <format>
#include <string>
#include <vector>

#include "chord.hpp"

struct Hotkey {
    bool passthrough{};
    bool repeat{};
    bool on_release{};
    std::vector<Chord> chords;

    std::strong_ordering operator<=>(const Hotkey& other) const = default;
};

template <>
struct std::formatter<Hotkey> : std::formatter<std::string_view> {
    auto format(const Hotkey& h, std::format_context& ctx) const {
        auto out = ctx.out();
        if (!h.chords.empty()) {
            out = std::format_to(out, "{}", h.chords[0]);
            for (size_t i = 1; i < h.chords.size(); ++i) {
                out = std::format_to(out, " ; {}", h.chords[i]);
            }
        }

        std::vector<std::string> flags;
        if (h.passthrough) flags.push_back("passthrough");
        if (h.repeat) flags.push_back("repeat");
        if (h.on_release) flags.push_back("on_release");

        if (!flags.empty()) {
            if (!h.chords.empty()) out = std::format_to(out, " ");
            out = std::format_to(out, "[{}", flags[0]);
            for (size_t i = 1; i < flags.size(); ++i) {
                out = std::format_to(out, ", {}", flags[i]);
            }
            out = std::format_to(out, "]");
        }

        return out;
    }
};
