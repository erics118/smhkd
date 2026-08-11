#include "safety_monitor.hpp"

#include <unordered_set>

void SafetyMonitor::evict(time_point now) {
    const auto cutoff = now - kConsumeWindow;
    while (!window_.empty() && window_.front().t < cutoff) {
        window_.pop_front();
    }
}

SafetyMonitor::Action SafetyMonitor::recordEvent(bool isKeyDown, bool consumed, uint32_t keycode, time_point now) {
    if (!isKeyDown) return Action::None;

    window_.push_back({now, keycode, consumed});
    evict(now);

    std::unordered_set<uint32_t> distinctConsumed;
    int passthrough = 0;
    for (const auto& ev : window_) {
        if (ev.consumed) {
            distinctConsumed.insert(ev.keycode);
        } else {
            passthrough++;
        }
    }

    // eating everything = many distinct keys consumed with almost no passthrough
    const auto total = static_cast<int>(window_.size());
    const bool fewPassthrough = passthrough * 100 <= total * kPassthroughTripPercent;
    if (static_cast<int>(distinctConsumed.size()) >= kDistinctConsumedTrip && fewPassthrough) {
        return Action::Trip;
    }
    return Action::None;
}

SafetyMonitor::Action SafetyMonitor::recordTimeout() {
    consecutiveTimeouts_++;
    if (consecutiveTimeouts_ >= kMaxConsecutiveTimeouts) {
        return Action::Trip;
    }
    return Action::ReEnable;
}

void SafetyMonitor::recordHealthy() {
    consecutiveTimeouts_ = 0;
}

void SafetyMonitor::reset() {
    window_.clear();
    consecutiveTimeouts_ = 0;
}
