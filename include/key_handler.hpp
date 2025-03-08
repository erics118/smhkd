#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "hotkey.hpp"

// TODO: support background process
// TODO: support loading from other files
// TODO: support notifying other services when a chord is pressed, so user have
// a visual indication of what is currently pressed
// TODO: trackpad gestures
// TODO: trackpad haptic feedback
struct KeyHandler {
    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    // map of hotkey to command
    std::map<Hotkey, std::string> hotkeys;

    // track last triggered chord for repeat detection
    std::optional<Chord> lastTriggeredChord;

    // track current chord sequence
    std::vector<Chord> currentChords;

    // track timing between chord presses
    double lastKeyPressTime{};

    explicit KeyHandler(std::map<Hotkey, std::string> hotkeys) : hotkeys{std::move(hotkeys)} {}

    // initialize the key handler
    bool init();

    // run the key handler
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
    bool checkAndExecuteSequence(const Chord& current);
};
