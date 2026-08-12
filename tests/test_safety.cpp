#include <CoreGraphics/CGEventTypes.h>

#include "doctest.h"
#include "runtime/hotkey_engine.hpp"
#include "runtime/safety_monitor.hpp"

namespace {

using Action = SafetyMonitor::Action;
using clock = SafetyMonitor::clock;

// feed n key-down events cycling through `distinct` keycodes, all consumed at the same instant
// returns the last action
Action feedConsumed(SafetyMonitor& m, int n, uint32_t distinct, clock::time_point now) {
    Action last = Action::None;
    for (int i = 0; i < n; i++) {
        last = m.recordEvent(true, true, static_cast<uint32_t>(i) % distinct, now);
    }
    return last;
}

}  // namespace

TEST_CASE("held layer spamming a few remaps never trips") {
    // hold a modifier, spam hjkl (4 distinct keys) 500 times, zero passthrough
    SafetyMonitor m;
    const auto now = clock::now();
    CHECK(feedConsumed(m, 500, 4, now) == Action::None);
}

TEST_CASE("eating many distinct keys with no passthrough trips") {
    SafetyMonitor m;
    const auto now = clock::now();
    Action last = Action::None;
    for (uint32_t k = 0; k < 25; k++) {
        last = m.recordEvent(true, true, k, now);
    }
    CHECK(last == Action::Trip);
}

TEST_CASE("normal typing (all passthrough) never trips") {
    SafetyMonitor m;
    const auto now = clock::now();
    Action last = Action::None;
    for (uint32_t k = 0; k < 60; k++) {
        last = m.recordEvent(true, false, k, now);
    }
    CHECK(last == Action::None);
}

TEST_CASE("many distinct consumed but with real passthrough does not trip") {
    // interleave 25 distinct consumed keys with 25 passthrough keys for 50% passthrough
    // well above the trip ceiling
    SafetyMonitor m;
    const auto now = clock::now();
    Action last = Action::None;
    for (uint32_t k = 0; k < 25; k++) {
        last = m.recordEvent(true, true, k, now);
        last = m.recordEvent(true, false, 1000 + k, now);
    }
    CHECK(last == Action::None);
}

TEST_CASE("key-up events are ignored by the consume-rate breaker") {
    SafetyMonitor m;
    const auto now = clock::now();
    Action last = Action::None;
    for (uint32_t k = 0; k < 40; k++) {
        last = m.recordEvent(false, true, k, now);
    }
    CHECK(last == Action::None);
}

TEST_CASE("old events fall out of the window") {
    SafetyMonitor m;
    const auto t0 = clock::now();
    // 15 distinct consumed now: below the trip threshold on its own
    for (uint32_t k = 0; k < 15; k++) {
        CHECK(m.recordEvent(true, true, k, t0) == Action::None);
    }
    // 4 seconds later the earlier window has aged out
    // 15 more distinct keys should still not trip because the first 15 were evicted
    const auto t1 = t0 + std::chrono::seconds(4);
    Action last = Action::None;
    for (uint32_t k = 100; k < 115; k++) {
        last = m.recordEvent(true, true, k, t1);
    }
    CHECK(last == Action::None);
}

TEST_CASE("timeout breaker re-enables then trips after the streak") {
    SafetyMonitor m;
    for (int i = 0; i < SafetyMonitor::kMaxConsecutiveTimeouts - 1; i++) {
        CHECK(m.recordTimeout() == Action::ReEnable);
    }
    CHECK(m.recordTimeout() == Action::Trip);
}

TEST_CASE("a healthy callback clears the timeout streak") {
    SafetyMonitor m;
    for (int i = 0; i < SafetyMonitor::kMaxConsecutiveTimeouts - 1; i++) {
        CHECK(m.recordTimeout() == Action::ReEnable);
    }
    m.recordHealthy();
    // streak reset, so the next timeout is a re-enable, not a trip
    CHECK(m.recordTimeout() == Action::ReEnable);
}

TEST_CASE("fail-open: unmatched input is never consumed") {
    // with no bindings, every event must pass through
    // a refactor that breaks this default is a lockout bug
    HotkeyEngine engine;
    engine.applyConfig({}, {}, {});

    for (uint32_t code = 0; code < 128; code++) {
        const Chord c{.keysym = {.keycode = code}, .modifiers = {.flags = 0}};
        CHECK(engine.handleEvent(c, kCGEventKeyDown, false, 0) == false);
        CHECK(engine.handleEvent(c, kCGEventKeyUp, false, 0) == false);
    }
}
