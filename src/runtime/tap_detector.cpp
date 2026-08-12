#include "tap_detector.hpp"

#include <unordered_set>

std::optional<Zone> TapDetector::onFrame(const std::vector<Touch>& touches, int64_t nowNs) {
    std::unordered_set<int> current;
    current.reserve(touches.size());
    for (const auto& t : touches) {
        current.insert(t.id);
    }

    const bool multi = touches.size() > 1;
    const int64_t timeoutNs = static_cast<int64_t>(config_.tapTimeoutMs) * 1'000'000;

    std::optional<Zone> tap;
    for (auto it = active_.begin(); it != active_.end();) {
        if (!current.contains(it->first)) {
            const Active& a = it->second;
            if (a.valid && a.startZone && (nowNs - a.downNs) <= timeoutNs) {
                tap = a.startZone;
            }
            it = active_.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& t : touches) {
        auto [it, inserted] = active_.try_emplace(t.id);
        Active& a = it->second;
        if (inserted) {
            a.downNs = nowNs;
            a.startZone = classifyZone(t.x, t.y, config_.cornerSizePct);
            a.valid = a.startZone.has_value();
        }
        if (multi) {
            a.valid = false;
        }
        if (classifyZone(t.x, t.y, config_.cornerSizePct) != a.startZone) {
            a.valid = false;
        }
    }
    return tap;
}

void TapDetector::reset() {
    active_.clear();
}
