#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "../input/zone.hpp"

struct Touch {
    int id;
    float x;
    float y;
};

// pure single-finger corner-tap detector, fed one frame of touches at a time
// a tap = one finger down and up within tapTimeoutMs, staying in the same corner,
// single-finger throughout
class TapDetector {
   public:
    struct Config {
        int cornerSizePct = 15;
        int tapTimeoutMs = 300;
    };

    void setConfig(Config config) { config_ = config; }

    std::optional<Zone> onFrame(const std::vector<Touch>& touches, int64_t nowNs);
    void reset();

   private:
    struct Active {
        int64_t downNs;
        std::optional<Zone> startZone;
        bool valid;
    };

    Config config_;
    std::unordered_map<int, Active> active_;
};
