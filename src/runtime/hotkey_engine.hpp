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
#include "../lang/interpreter.hpp"

class HotkeyEngine {
   public:
    void applyConfig(std::map<Hotkey, std::string> hotkeys, std::vector<RemapBinding> remaps, ConfigProperties config);
    [[nodiscard]] bool handleEvent(const Chord& current, CGEventType type, bool isRepeat, std::string_view frontProcess);
    void reset();
    static void synthesizeKeyPress(const Chord& target);

    static constexpr int64_t SYNTHETIC_REMAP_TAG = 0x534d484b44;

   private:
    std::map<Hotkey, std::string> hotkeys_;
    std::vector<RemapBinding> remaps_;
    ConfigProperties config_;
    std::optional<Chord> lastChord_;
    std::vector<Chord> sequence_;
    std::chrono::time_point<std::chrono::system_clock> lastPressTime_;

    void clearSequence();
    [[nodiscard]] bool handleSequence(const Chord& chord);
    [[nodiscard]] bool applyRemap(const Chord& chord, CGEventType type);
    [[nodiscard]] bool isBlacklisted(std::string_view frontApp) const;
    [[nodiscard]] static std::string toLower(std::string_view s);
    static void postKeyEvent(const Chord& target, bool keyDown);
};
