#include "touch_handler.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "multitouch_support.hpp"
#include "tap_detector.hpp"

namespace {

int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::atomic<int> g_totalFingers{0};

// guards state shared between the MultitouchSupport callback thread and the run loop
std::mutex g_touchMtx;
std::unordered_map<int, int> g_perDevice;
std::unordered_map<int, TapDetector> g_detectors;
TapDetector::Config g_tapConfig;
std::function<void(Zone)> g_tapCallback;

// most recent single-finger corner contact, read from the event tap for suppression
std::atomic<int> g_lastCornerZone{-1};
std::atomic<int64_t> g_lastCornerNs{0};

CFMutableArrayRef g_deviceList = nullptr;
bool g_started = false;

int contactCallback(int device, Finger* fingers, int nFingers, double /*timestamp*/, int /*frame*/) {
    std::lock_guard<std::mutex> lock(g_touchMtx);
    if (!g_started) return 0;

    const int64_t now = nowNs();

    g_perDevice[device] = nFingers;
    int total = 0;
    for (const auto& [id, count] : g_perDevice) {
        total += count;
    }
    g_totalFingers.store(total, std::memory_order_release);

    std::vector<Touch> touches;
    touches.reserve(static_cast<size_t>(nFingers));
    for (int i = 0; i < nFingers; i++) {
        touches.push_back({fingers[i].identifier, fingers[i].normalized.position.x, fingers[i].normalized.position.y});
    }

    // recent corner contact (single finger only) for click suppression
    if (nFingers == 1) {
        if (auto zone = classifyZone(touches[0].x, touches[0].y, g_tapConfig.cornerSizePct)) {
            g_lastCornerZone.store(static_cast<int>(*zone), std::memory_order_release);
            g_lastCornerNs.store(now, std::memory_order_release);
        }
    }

    // per-device detector so finger ids from different pads never collide
    auto& detector = g_detectors[device];
    detector.setConfig(g_tapConfig);
    if (auto zone = detector.onFrame(touches, now)) {
        if (g_tapCallback) g_tapCallback(*zone);
    }
    return 0;
}

}  // namespace

void touch::start() {
    std::lock_guard<std::mutex> lock(g_touchMtx);
    if (g_started) return;

    // MultitouchSupport logs a device-recognition line on start
    // redirect stdout/stderr to /dev/null around device setup to silence it
    fflush(stdout);
    fflush(stderr);
    const int savedOut = dup(STDOUT_FILENO);
    const int savedErr = dup(STDERR_FILENO);
    const int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
    }

    g_deviceList = MTDeviceCreateList();
    if (g_deviceList) {
        const CFIndex n = CFArrayGetCount(g_deviceList);
        for (CFIndex i = 0; i < n; i++) {
            auto dev = const_cast<MTDeviceRef>(CFArrayGetValueAtIndex(g_deviceList, i));
            MTRegisterContactFrameCallback(dev, contactCallback);
            MTDeviceStart(dev, 0);
        }
        g_started = true;
    }

    fflush(stdout);
    fflush(stderr);
    if (devnull != -1) close(devnull);
    if (savedOut != -1) {
        dup2(savedOut, STDOUT_FILENO);
        close(savedOut);
    }
    if (savedErr != -1) {
        dup2(savedErr, STDERR_FILENO);
        close(savedErr);
    }
}

void touch::stop() {
    std::lock_guard<std::mutex> lock(g_touchMtx);
    if (!g_started) return;

    const CFIndex n = CFArrayGetCount(g_deviceList);
    for (CFIndex i = 0; i < n; i++) {
        auto dev = const_cast<MTDeviceRef>(CFArrayGetValueAtIndex(g_deviceList, i));
        MTDeviceStop(dev);
    }
    CFRelease(g_deviceList);
    g_deviceList = nullptr;
    g_perDevice.clear();
    g_detectors.clear();
    g_lastCornerZone.store(-1, std::memory_order_release);
    g_totalFingers.store(0, std::memory_order_release);
    g_started = false;
}

int touch::fingerCount() {
    return g_totalFingers.load(std::memory_order_acquire);
}

void touch::setTapConfig(int cornerSizePct, int tapTimeoutMs) {
    std::lock_guard<std::mutex> lock(g_touchMtx);
    g_tapConfig = {.cornerSizePct = cornerSizePct, .tapTimeoutMs = tapTimeoutMs};
}

void touch::setTapCallback(std::function<void(Zone)> callback) {
    std::lock_guard<std::mutex> lock(g_touchMtx);
    g_tapCallback = std::move(callback);
}

std::optional<Zone> touch::recentCornerZone(int64_t maxAgeNs) {
    const int zone = g_lastCornerZone.load(std::memory_order_acquire);
    if (zone < 0) return std::nullopt;
    const int64_t last = g_lastCornerNs.load(std::memory_order_acquire);
    if (nowNs() - last > maxAgeNs) return std::nullopt;
    return static_cast<Zone>(zone);
}
