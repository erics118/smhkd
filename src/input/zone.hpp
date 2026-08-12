#pragma once

#include <optional>
#include <string>

enum class Zone {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

std::optional<Zone> parseZone(const std::string& name);
const char* zoneName(Zone zone);

// classify a normalized (0..1) trackpad position into a corner zone
// origin is bottom-left: y=1 is the top edge, y=0 the bottom, x=1 the right
std::optional<Zone> classifyZone(float x, float y, int cornerSizePct);
