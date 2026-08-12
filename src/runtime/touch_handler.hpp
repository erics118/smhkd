#pragma once

#include <functional>
#include <optional>

#include "../input/zone.hpp"

namespace touch {

void start();
void stop();

// live total fingers across all trackpads
int fingerCount();

void setTapConfig(int cornerSizePct, int tapTimeoutMs);
void setTapCallback(std::function<void(Zone)> callback);

// corner of a single-finger contact seen within maxAgeNs, for click suppression
std::optional<Zone> recentCornerZone(int64_t maxAgeNs);

}  // namespace touch
