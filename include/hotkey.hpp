#pragma once

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <print>
#include <string>
#include <vector>

#include "chord.hpp"

// TODO: mouse, up, down, buttons 1-5
// TODO: trackpad number fingers

// TODO: simultaneous keys, ie within x milliseconds
// TODO: use keys as modifiers, by tracking what is held down
struct Hotkey {
    bool passthrough{};
    bool repeat{};
    bool on_release{};

    std::vector<Chord> chords;

    std::strong_ordering operator<=>(const Hotkey& other) const {
        if (passthrough != other.passthrough) {
            return passthrough <=> other.passthrough;
        }
        if (repeat != other.repeat) {
            return repeat <=> other.repeat;
        }
        if (on_release != other.on_release) {
            return on_release <=> other.on_release;
        }

        if (chords.size() != other.chords.size()) {
            return chords.size() <=> other.chords.size();
        }

        for (size_t i = 0; i < chords.size(); i++) {
            if (chords[i] != other.chords[i]) {
                return chords[i] <=> other.chords[i];
            }
        }

        return std::strong_ordering::equal;
    }

    Hotkey() {
        chords.push_back(Chord{});
    }

    explicit Hotkey(Chord chord) {
        chords.push_back(chord);
    }
};

template <>
struct std::formatter<Hotkey> : std::formatter<std::string_view> {
    auto format(Hotkey hk, std::format_context& ctx) const {
        // print out each part
        std::string str;

        for (size_t i = 0; i < hk.chords.size(); i++) {
            if (i > 0) str += " ; ";
            str += std::format("{}", hk.chords[i]);
        }

        std::string flags;

        if (hk.passthrough) flags += "Passthrough";
        if (hk.repeat) flags += (flags.empty() ? "" : " ") + std::string{"Repeat"};
        if (hk.on_release) flags += (flags.empty() ? "" : " ") + std::string{"OnRelease"};
        if (!flags.empty()) str += " (" + flags + ")";

        return std::format_to(ctx.out(), "{}", str);
    }
};
