#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "hotkey.hpp"

// TODO: support background process
// TODO: support loading from other files
struct Service {
    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    // map of hotkey to command
    std::unordered_map<Hotkey, std::string> hotkeys;

    // track last triggered hotkey for repeat detection
    std::optional<Hotkey> lastTriggeredHotkey;

    // track current chord sequence
    std::vector<Hotkey> currentSequence;

    // track timing between chord presses
    double lastKeyPressTime{};

    explicit Service(std::unordered_map<Hotkey, std::string> hotkeys) : hotkeys{std::move(hotkeys)} {}

    // initialize the service
    bool init();

    // run the service
    void run() const;

    // load the config file
    bool loadConfig(const std::string& configFile);

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
    bool checkAndExecuteSequence(const Hotkey& current);
};
