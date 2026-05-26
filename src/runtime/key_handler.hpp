#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <cctype>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../input/chord.hpp"
#include "../input/hotkey.hpp"
#include "../lang/interpreter.hpp"

struct KeyHandler {
    std::string configFileName;

    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    std::map<Hotkey, std::string> hotkeys;
    ConfigProperties config;

    std::optional<Chord> lastTriggeredChord;
    std::vector<Chord> currentChords;
    std::chrono::time_point<std::chrono::system_clock> lastKeyPressTime;

    explicit KeyHandler(const std::string& configFileName) : configFileName(configFileName) {
        loadConfig(configFileName);
    }

    bool init();
    void run() const;
    void loadConfig(const std::string& config_file);
    bool setupEventTap();
    [[nodiscard]] static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon);
    [[nodiscard]] bool handleKeyEvent(CGEventRef event, CGEventType type);

    void reload() {
        clearSequence();
        loadConfig(configFileName);
    }

   private:
    void clearSequence();
    bool checkAndExecuteSequence(const Chord& current);
    bool isBlacklistedProcess() const;
    static std::string getFrontProcessName();
    static std::string toLower(std::string s);
};
