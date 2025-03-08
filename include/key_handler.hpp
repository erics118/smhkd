#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "config_properties.hpp"
#include "hotkey.hpp"

// TODO: support loading from other files
// TODO: support notifying other services when a chord is pressed, so user have
// a visual indication of what is currently pressed
// TODO: trackpad gestures
// TODO: trackpad haptic feedback
struct KeyHandler {
    std::string configFileName;

    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    // map of hotkey to command
    std::map<Hotkey, std::string> hotkeys;

    // Maximum time between chord presses (in milliseconds)
    ConfigProperties config;

    // track last triggered chord for repeat detection
    std::optional<Chord> lastTriggeredChord;

    // track current chord sequence
    std::vector<Chord> currentChords;
    // track timing between chord presses
    std::chrono::time_point<std::chrono::system_clock> lastKeyPressTime;

    explicit KeyHandler(const std::string& configFileName) : configFileName{configFileName} {
        loadConfig(configFileName);
    }

    // explicit KeyHandler(std::map<Hotkey, std::string> hotkeys, ConfigProperties config)
    //     : hotkeys{std::move(hotkeys)}, config{config} {}

    // void reload(std::map<Hotkey, std::string> hotkeys, ConfigProperties config) {
    //     this->hotkeys = std::move(hotkeys);
    //     this->config = config;
    // }

    void reload() {
        loadConfig(configFileName);
    }

    // initialize the key handler
    bool init();

    // run the key handler
    void run() const;

    // load the config file
    void loadConfig(const std::string& config_file);

    // setup the event tap
    bool setupEventTap();

    // event callback
    [[nodiscard]] static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon);

    // handle a key event
    [[nodiscard]] bool handleKeyEvent(CGEventRef event, CGEventType type);

   private:
    // clear the current sequence
    void clearSequence();

    // check if sequence matches and execute
    bool checkAndExecuteSequence(const Chord& current);
};
