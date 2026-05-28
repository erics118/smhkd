#pragma once

#include <CoreGraphics/CGEventTypes.h>

#include <filesystem>

#include "hotkey_engine.hpp"

class KeyHandler {
   private:
    std::filesystem::path configFile;

    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};
    HotkeyEngine engine;

    bool setupEventTap();
    [[nodiscard]] static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon);
    [[nodiscard]] bool handleKeyEvent(CGEventRef event, CGEventType type);
    void loadConfig(const std::filesystem::path& configFile);

   public:
    explicit KeyHandler(std::filesystem::path configFile) : configFile(std::move(configFile)) {
        loadConfig(this->configFile);
    }

    bool init();
    void run() const;

    void reload() {
        engine.reset();
        loadConfig(configFile);
    }
};
