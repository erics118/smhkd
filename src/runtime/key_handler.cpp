#include "key_handler.hpp"

#include <unistd.h>

#include <chrono>

#include "../common/log.hpp"
#include "../common/signpost.hpp"
#include "../lang/config_loader.hpp"
#include "../runtime/service.hpp"

namespace {

os_log_t signpostLog() {
    static os_log_t log = os_log_create("dev.smhkd", "PointsOfInterest");
    return log;
}

int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

KeyHandler::~KeyHandler() {
    watchdogStop.store(true, std::memory_order_release);
    if (watchdog.joinable()) watchdog.join();
}

bool KeyHandler::init() {
    runLoop = CFRunLoopGetCurrent();
    if (!runLoop) return false;
    debug("run loop initialized");
    if (!setupEventTap()) return false;
    debug("event tap initialized");
    startWatchdog();
    debug("watchdog started");
    return true;
}

bool KeyHandler::setupEventTap() {
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);
    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, eventCallback, this);
    if (!tap) fatal("failed to create event tap");
    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);
    eventTap = tap;
    CFRelease(runLoopSource);
    return true;
}

CGEventRef KeyHandler::eventCallback(CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* refcon) {
    auto* keyHandler = static_cast<KeyHandler*>(refcon);

    keyHandler->callbackStartNs.store(nowNs(), std::memory_order_release);

    // fail open: on any error, pass the event through untouched, never consume
    CGEventRef result = event;
    try {
        if (type == kCGEventTapDisabledByUserInput) {
            // macOS disabled for secure input (password fields), not our fault
            CGEventTapEnable(keyHandler->eventTap, true);
        } else if (type == kCGEventTapDisabledByTimeout) {
            // a callback overran the OS timeout, the breaker decides recover vs bail
            switch (keyHandler->safety.recordTimeout()) {
                case SafetyMonitor::Action::Trip:
                    error("event tap timing out repeatedly, exiting for a clean restart");
                    CGEventTapEnable(keyHandler->eventTap, false);
                    _exit(1);
                case SafetyMonitor::Action::ReEnable:
                case SafetyMonitor::Action::None:
                    warn("event tap disabled by timeout; re-enabled");
                    CGEventTapEnable(keyHandler->eventTap, true);
                    break;
            }
        } else {
            os_log_t log = signpostLog();
            os_signpost_id_t spid = SIGNPOST_GENERATE(log);
            SIGNPOST_BEGIN(log, spid, "eventCallback", "type=%d", static_cast<int>(type));
            const bool consumed = keyHandler->handleKeyEvent(event, type);
            SIGNPOST_END(log, spid, "eventCallback", "consumed=%d", consumed ? 1 : 0);

            const bool isKeyDown = type == kCGEventKeyDown;
            const auto keycode = static_cast<uint32_t>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
            if (keyHandler->safety.recordEvent(isKeyDown, consumed, keycode, SafetyMonitor::clock::now()) == SafetyMonitor::Action::Trip) {
                error("event tap consuming nearly all input, exiting for a clean restart");
                CGEventTapEnable(keyHandler->eventTap, false);
                _exit(1);
            }
            keyHandler->safety.recordHealthy();

            result = consumed ? nullptr : event;
        }
    } catch (...) {
        result = event;
    }

    keyHandler->callbackStartNs.store(0, std::memory_order_release);
    keyHandler->callbackGen.fetch_add(1, std::memory_order_release);
    return result;
}

bool KeyHandler::handleKeyEvent(CGEventRef event, CGEventType type) {
    if (CGEventGetIntegerValueField(event, kCGEventSourceUserData) == HotkeyEngine::SYNTHETIC_REMAP_TAG) {
        return false;
    }

    auto keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    CGEventFlags flags = CGEventGetFlags(event);
    bool isRepeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

    debug("TRACE event type={} keycode={} flags={:#x}", static_cast<int>(type), keyCode, flags);

    Chord current{
        .keysym = {.keycode = keyCode},
        .modifiers = eventModifierFlagsToHotkeyFlags(flags),
    };

    auto exitChord = Chord{
        .keysym = {.keycode = 8},
        .modifiers = {.flags = Hotkey_Flag_RAlt},
    };
    if (exitChord.isActivatedBy(current)) {
        error("exit hotkey, ralt-c, detected, ending program");
        service::stop();
        std::exit(1);
    }

    return engine.handleEvent(current, type, isRepeat);
}

void KeyHandler::startWatchdog() {
    watchdog = std::thread(&KeyHandler::watchdogLoop, this);
}

void KeyHandler::watchdogLoop() {
    const auto softNs = std::chrono::duration_cast<std::chrono::nanoseconds>(SafetyMonitor::kSoftOverrun).count();
    const auto hardNs = std::chrono::duration_cast<std::chrono::nanoseconds>(SafetyMonitor::kHardOverrun).count();

    bool softDisabled = false;
    uint64_t genWhenDisabled = 0;

    while (!watchdogStop.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // sample gen with start before acting
        // if the callback exits after this, gen moves and the soft path below re-enables
        // capturing gen after the disable would miss that exit and leave the tap disabled
        const int64_t start = callbackStartNs.load(std::memory_order_acquire);
        const uint64_t gen = callbackGen.load(std::memory_order_acquire);
        if (start != 0) {
            const int64_t elapsed = nowNs() - start;
            if (elapsed > hardNs && callbackStartNs.load(std::memory_order_acquire) == start) {
                // deadlock the run loop can't recover from
                // _exit lets launchd restart clean
                CGEventTapEnable(eventTap, false);
                _exit(1);
            }
            if (elapsed > softNs && !softDisabled) {
                // transient slowness: drop the tap so input flows now
                genWhenDisabled = gen;
                CGEventTapEnable(eventTap, false);
                softDisabled = true;
            }
        }

        // the stuck callback has since returned (generation moved): restore the tap
        if (softDisabled && callbackGen.load(std::memory_order_acquire) != genWhenDisabled) {
            CGEventTapEnable(eventTap, true);
            softDisabled = false;
        }
    }
}

void KeyHandler::run() const {
    if (!runLoop) return;
    info("running key handler");
    CFRunLoopRun();
}

void KeyHandler::loadConfig(const std::filesystem::path& configFile) {
    info("config file set to: {}", configFile.string());
    auto result = ConfigLoader::loadFromFile(configFile);
    if (result.fileError) {
        warn("config error: {}", *result.fileError);
    }
    for (const auto& parse_error : result.parseErrors) {
        warn("parse error at line {}, column {}: {}", parse_error.row, parse_error.col, parse_error.message);
    }
    for (const auto& interpreter_error : result.interpreterErrors) {
        warn("config error: {}", interpreter_error.message);
    }
    engine.applyConfig(std::move(result.bindings), std::move(result.config));
}
