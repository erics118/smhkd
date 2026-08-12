#pragma once

#include <CoreGraphics/CGEventTypes.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../input/chord.hpp"
#include "../input/hotkey.hpp"
#include "../input/zone.hpp"
#include "../lang/interpreter.hpp"

class HotkeyEngine {
   public:
    void applyConfig(std::vector<Binding> bindings, std::vector<TapBinding> tapBindings, ConfigProperties config);
    [[nodiscard]] bool handleEvent(const Chord& current, CGEventType type, bool isRepeat, int fingerCount);

    // run a matching corner-tap binding; returns whether one fired
    [[nodiscard]] bool handleTap(Zone zone, ModifierFlags mods);
    // whether a corner-tap binding exists for this zone + modifiers (for click suppression)
    [[nodiscard]] bool hasTapBinding(Zone zone, ModifierFlags mods) const;

    void reset();
    static void synthesizeKeyPress(const Chord& target);

    static constexpr int64_t SYNTHETIC_REMAP_TAG = 0x534d484b44;

   private:
    std::vector<Binding> bindings_;
    std::vector<TapBinding> tapBindings_;
    ConfigProperties config_;
    std::optional<Chord> lastChord_;
    std::vector<Chord> sequence_;
    std::chrono::time_point<std::chrono::system_clock> lastPressTime_;

    void clearSequence();
    void runSequenceCommand() const;
    [[nodiscard]] bool handleSequence(const Chord& chord, int fingerCount);
    [[nodiscard]] bool isBlacklisted(std::string_view processName) const;
    void executeHotkeyCommand(const std::string& command) const;
    static void postKeyEvent(const Chord& target, bool keyDown);
};
