#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
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
struct Service {
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

    // track currently held keys
    std::map<Keysym, std::chrono::steady_clock::time_point> heldKeysyms;

    // track delayed key events that might be held modifiers
    struct DelayedKeyEvent {
        Keysym keysym;
        std::chrono::steady_clock::time_point pressTime;
        bool consumed{false};  // whether this key was used as a modifier
    };
    std::map<Keysym, DelayedKeyEvent> delayedKeysyms;

    explicit Service(std::map<Hotkey, std::string> hotkeys) : hotkeys{std::move(hotkeys)} {}

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

    bool keyEventReturn(bool returnValue);

    // handle a key event
    [[nodiscard]] bool handleKeyEvent(CGEventRef event, CGEventType type);

   private:
    // clear the current sequence
    void clearSequence();

    // check if sequence matches and execute
    bool checkAndExecuteSequence(const Chord& current);

    // check if a key is being held as a modifier
    [[nodiscard]] bool isKeyHeldAsModifier(Keysym keysym) const;

    // check if a key could be used as a held modifier
    [[nodiscard]] bool isPotentialHeldModifier(Keysym keysym) const;

    // process any delayed events that have exceeded the hold threshold
    void processDelayedEvents();

    // send a delayed event to the system
    void sendDelayedEvent(Keysym keysym);
};
