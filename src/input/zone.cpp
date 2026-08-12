#include "zone.hpp"

std::optional<Zone> parseZone(const std::string& name) {
    if (name == "tl") return Zone::TopLeft;
    if (name == "tr") return Zone::TopRight;
    if (name == "bl") return Zone::BottomLeft;
    if (name == "br") return Zone::BottomRight;
    return std::nullopt;
}

const char* zoneName(Zone zone) {
    switch (zone) {
        case Zone::TopLeft: return "tl";
        case Zone::TopRight: return "tr";
        case Zone::BottomLeft: return "bl";
        case Zone::BottomRight: return "br";
    }
    return "?";
}

std::optional<Zone> classifyZone(float x, float y, int cornerSizePct) {
    const float frac = static_cast<float>(cornerSizePct) / 100.0F;
    const bool left = x <= frac;
    const bool right = x >= 1.0F - frac;
    const bool bottom = y <= frac;
    const bool top = y >= 1.0F - frac;

    if (top && left) return Zone::TopLeft;
    if (top && right) return Zone::TopRight;
    if (bottom && left) return Zone::BottomLeft;
    if (bottom && right) return Zone::BottomRight;
    return std::nullopt;
}
