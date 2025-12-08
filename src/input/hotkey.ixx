module;

#include <CoreGraphics/CGEventTypes.h>

#include <compare>
#include <format>
#include <string>
#include <vector>

export module hotkey;
import chord;
import modifier;

export struct Hotkey {
    bool passthrough{};
    bool repeat{};
    bool on_release{};
    std::vector<Chord> chords;

    std::strong_ordering operator<=>(const Hotkey& other) const = default;
};
