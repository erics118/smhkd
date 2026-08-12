#include <vector>

#include "doctest.h"
#include "input/zone.hpp"
#include "runtime/tap_detector.hpp"

namespace {

int64_t ms(int64_t n) { return n * 1'000'000; }

}  // namespace

TEST_CASE("parseZone maps the four corner names") {
    CHECK(parseZone("tl") == Zone::TopLeft);
    CHECK(parseZone("tr") == Zone::TopRight);
    CHECK(parseZone("bl") == Zone::BottomLeft);
    CHECK(parseZone("br") == Zone::BottomRight);
    CHECK_FALSE(parseZone("xx").has_value());
    CHECK_FALSE(parseZone("t").has_value());
}

TEST_CASE("classifyZone uses bottom-left origin (y=1 is top)") {
    CHECK(classifyZone(0.95F, 0.95F, 15) == Zone::TopRight);
    CHECK(classifyZone(0.05F, 0.95F, 15) == Zone::TopLeft);
    CHECK(classifyZone(0.95F, 0.05F, 15) == Zone::BottomRight);
    CHECK(classifyZone(0.05F, 0.05F, 15) == Zone::BottomLeft);
}

TEST_CASE("classifyZone returns none for the center and edges outside a corner") {
    CHECK_FALSE(classifyZone(0.5F, 0.5F, 15).has_value());
    CHECK_FALSE(classifyZone(0.84F, 0.95F, 15).has_value());
    CHECK_FALSE(classifyZone(0.95F, 0.5F, 15).has_value());
}

TEST_CASE("classifyZone corner size scales the patch") {
    CHECK_FALSE(classifyZone(0.80F, 0.80F, 15).has_value());
    CHECK(classifyZone(0.80F, 0.80F, 25) == Zone::TopRight);
}

TEST_CASE("a quick single-finger corner tap is detected") {
    TapDetector d;
    d.setConfig({.cornerSizePct = 15, .tapTimeoutMs = 300});
    CHECK_FALSE(d.onFrame({{1, 0.95F, 0.95F}}, ms(0)).has_value());
    CHECK(d.onFrame({}, ms(50)) == Zone::TopRight);
}

TEST_CASE("a tap slower than the timeout is not detected") {
    TapDetector d;
    d.setConfig({.cornerSizePct = 15, .tapTimeoutMs = 300});
    CHECK_FALSE(d.onFrame({{1, 0.95F, 0.95F}}, ms(0)).has_value());
    CHECK_FALSE(d.onFrame({}, ms(400)).has_value());
}

TEST_CASE("a finger that leaves the corner before lifting is not a tap") {
    TapDetector d;
    d.setConfig({.cornerSizePct = 15, .tapTimeoutMs = 300});
    CHECK_FALSE(d.onFrame({{1, 0.95F, 0.95F}}, ms(0)).has_value());
    CHECK_FALSE(d.onFrame({{1, 0.50F, 0.50F}}, ms(20)).has_value());
    CHECK_FALSE(d.onFrame({}, ms(40)).has_value());
}

TEST_CASE("a center tap is not a corner tap") {
    TapDetector d;
    d.setConfig({.cornerSizePct = 15, .tapTimeoutMs = 300});
    CHECK_FALSE(d.onFrame({{1, 0.50F, 0.50F}}, ms(0)).has_value());
    CHECK_FALSE(d.onFrame({}, ms(50)).has_value());
}

TEST_CASE("a multi-finger tap in a corner is not a single-finger tap") {
    TapDetector d;
    d.setConfig({.cornerSizePct = 15, .tapTimeoutMs = 300});
    CHECK_FALSE(d.onFrame({{1, 0.95F, 0.95F}, {2, 0.90F, 0.90F}}, ms(0)).has_value());
    CHECK_FALSE(d.onFrame({}, ms(50)).has_value());
}

TEST_CASE("a finger starting outside a corner never becomes a tap") {
    TapDetector d;
    d.setConfig({.cornerSizePct = 15, .tapTimeoutMs = 300});
    CHECK_FALSE(d.onFrame({{1, 0.50F, 0.50F}}, ms(0)).has_value());
    CHECK_FALSE(d.onFrame({{1, 0.95F, 0.95F}}, ms(20)).has_value());
    CHECK_FALSE(d.onFrame({}, ms(40)).has_value());
}
