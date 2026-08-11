#pragma once

#include <CoreGraphics/CGEventTypes.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <thread>

#include "hotkey_engine.hpp"
#include "safety_monitor.hpp"

class KeyHandler {
   private:
    std::filesystem::path configFile;

    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};
    HotkeyEngine engine;

    SafetyMonitor safety;

    // watchdog: an independent thread that force-disables the tap if a callback
    // runs too long, so a true deadlock in the run loop can never lock out input
    std::thread watchdog;
    std::atomic<bool> watchdogStop{false};
    // steady-clock nanoseconds at callback entry, 0 when idle
    std::atomic<int64_t> callbackStartNs{0};
    // bumped when a callback returns, so the watchdog can tell a stuck callback finished
    std::atomic<uint64_t> callbackGen{0};

    bool setupEventTap();
    void startWatchdog();
    void watchdogLoop();
    [[nodiscard]] static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon);
    [[nodiscard]] bool handleKeyEvent(CGEventRef event, CGEventType type);
    void loadConfig(const std::filesystem::path& configFile);

   public:
    explicit KeyHandler(std::filesystem::path configFile) : configFile(std::move(configFile)) {
        loadConfig(this->configFile);
    }

    ~KeyHandler();
    KeyHandler(const KeyHandler&) = delete;
    KeyHandler& operator=(const KeyHandler&) = delete;
    KeyHandler(KeyHandler&&) = delete;
    KeyHandler& operator=(KeyHandler&&) = delete;

    bool init();
    void run() const;

    void reload() {
        engine.reset();
        loadConfig(configFile);
    }
};
