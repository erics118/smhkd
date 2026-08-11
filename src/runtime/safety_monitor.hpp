#pragma once

#include <chrono>
#include <cstdint>
#include <deque>

class SafetyMonitor {
   public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    enum class Action {
        None,
        ReEnable,  // transient: re-enable the tap and keep running
        Trip,      // fatal: force-disable and _exit for a clean launchd restart
    };

    // key-up ignored so remaps aren't double-counted
    Action recordEvent(bool isKeyDown, bool consumed, uint32_t keycode, time_point now);

    Action recordTimeout();

    // clears the timeout streak
    void recordHealthy();

    void reset();

    // consume-rate breaker (Mode B)
    static constexpr int kDistinctConsumedTrip = 20;
    static constexpr int kPassthroughTripPercent = 10;
    static constexpr auto kConsumeWindow = std::chrono::seconds(3);

    // timeout breaker (Mode A)
    static constexpr int kMaxConsecutiveTimeouts = 5;

    // watchdog tiers
    // soft: drop the tap while a callback overruns
    // hard: on a deadlock, force _exit
    static constexpr auto kSoftOverrun = std::chrono::milliseconds(250);
    static constexpr auto kHardOverrun = std::chrono::milliseconds(3000);

   private:
    struct Ev {
        time_point t;
        uint32_t keycode;
        bool consumed;
    };

    std::deque<Ev> window_;
    int consecutiveTimeouts_{};

    void evict(time_point now);
};
