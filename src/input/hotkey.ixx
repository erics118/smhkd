module;

#include <CoreGraphics/CGEventTypes.h>

#include <compare>
#include <vector>

export module smhkd.hotkey;
import smhkd.chord;
import smhkd.modifier;

export struct Hotkey {
    bool passthrough{};
    bool repeat{};
    bool on_release{};
    std::vector<Chord> chords;

    std::strong_ordering operator<=>(const Hotkey& other) const {
        if (passthrough != other.passthrough) return passthrough <=> other.passthrough;
        if (repeat != other.repeat) return repeat <=> other.repeat;
        if (on_release != other.on_release) return on_release <=> other.on_release;
        if (chords.size() != other.chords.size()) return chords.size() <=> other.chords.size();
        for (size_t i = 0; i < chords.size(); i++) {
            if (chords[i] != other.chords[i]) return chords[i] <=> other.chords[i];
        }
        return std::strong_ordering::equal;
    }
};
