#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <optional>
#include <string>
#include <vector>

#include "hotkey.hpp"

struct Service {
    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};
    std::vector<Hotkey> hotkeys;
    std::optional<Hotkey> lastTriggeredHotkey;  // Track last triggered hotkey for repeat detection

    explicit Service(std::vector<Hotkey> hotkeys) : hotkeys{std::move(hotkeys)} {}

    bool init();

    void run() const;

    bool loadConfig(const std::string& configFile);

    bool setupEventTap();

    static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon);

    bool handleKeyEvent(CGEventRef event, CGEventType type);
};
