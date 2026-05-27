#pragma once

#include <CoreGraphics/CGEventTypes.h>

#include <chrono>
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
    void applyConfig(std::map<Hotkey, std::string> hotkeys, ConfigProperties config);
    [[nodiscard]] bool handleEvent(const Chord& current, CGEventType type, bool isRepeat, std::string_view frontProcessName);
    void reset();

   private:
    std::map<Hotkey, std::string> hotkeys_;
    ConfigProperties config_;
    std::optional<Chord> lastTriggeredChord_;
    std::vector<Chord> currentChords_;
    std::chrono::time_point<std::chrono::system_clock> lastKeyPressTime_;

    void clearSequence();
    [[nodiscard]] bool checkAndExecuteSequence(const Chord& current);
    [[nodiscard]] bool isBlacklistedProcess(std::string_view frontProcessName) const;
    [[nodiscard]] static std::string toLower(std::string_view s);
};
