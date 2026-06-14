#include "key_handler.hpp"

#include "../common/log.hpp"
#include "../common/signpost.hpp"
#include "../lang/config_loader.hpp"
#include "../runtime/service.hpp"

namespace {

os_log_t signpostLog() {
    static os_log_t log = os_log_create("dev.smhkd", "PointsOfInterest");
    return log;
}

}  // namespace

bool KeyHandler::init() {
    runLoop = CFRunLoopGetCurrent();
    if (!runLoop) return false;
    debug("run loop initialized");
    if (!setupEventTap()) return false;
    debug("event tap initialized");
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

    // if disabled, skip processing the event and re-enable the event tap
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        CGEventTapEnable(keyHandler->eventTap, true);
        warn("event tap was disabled ({}); re-enabled", static_cast<int>(type));
        return event;
    }

    os_log_t log = signpostLog();
    os_signpost_id_t spid = SIGNPOST_GENERATE(log);
    SIGNPOST_BEGIN(log, spid, "eventCallback", "type=%d", static_cast<int>(type));
    bool consumed = keyHandler->handleKeyEvent(event, type);
    SIGNPOST_END(log, spid, "eventCallback", "consumed=%d", consumed ? 1 : 0);

    if (consumed) return nullptr;
    return event;
}

bool KeyHandler::handleKeyEvent(CGEventRef event, CGEventType type) {
    if (CGEventGetIntegerValueField(event, kCGEventSourceUserData) == HotkeyEngine::SYNTHETIC_REMAP_TAG) {
        return false;
    }

    auto keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    CGEventFlags flags = CGEventGetFlags(event);
    bool isRepeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

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
    engine.applyConfig(std::move(result.hotkeys), std::move(result.remaps), std::move(result.config));
}
