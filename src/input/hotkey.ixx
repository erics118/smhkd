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

    std::strong_ordering operator<=>(const Hotkey& other) const = default;
};
