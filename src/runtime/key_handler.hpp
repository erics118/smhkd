#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <string>

#include "hotkey_engine.hpp"

struct KeyHandler {
    std::string configFileName;

    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};
    HotkeyEngine engine;

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
        engine.reset();
        loadConfig(configFileName);
    }

   private:
    static std::string getFrontProcessName();
};
