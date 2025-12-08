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

export template <>
struct std::formatter<Hotkey> : std::formatter<std::string_view> {
    auto format(const Hotkey& h, std::format_context& ctx) const {
        std::string result;

        // Format chords
        if (!h.chords.empty()) {
            result = std::format("{}", h.chords[0]);
            for (size_t i = 1; i < h.chords.size(); ++i) {
                result += " ; " + std::format("{}", h.chords[i]);
            }
        }

        // Add flags if set
        std::vector<std::string> flags;
        if (h.passthrough) flags.push_back("passthrough");
        if (h.repeat) flags.push_back("repeat");
        if (h.on_release) flags.push_back("on_release");

        if (!flags.empty()) {
            if (!result.empty()) result += " ";
            result += "[";
            result += flags[0];
            for (size_t i = 1; i < flags.size(); ++i) {
                result += ", " + flags[i];
            }
            result += "]";
        }

        return std::format_to(ctx.out(), "{}", result);
    }
};
