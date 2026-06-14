#include "hotkey_engine.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>

#include "../common/command.hpp"
#include "../common/front_app.hpp"
#include "../common/log.hpp"
#include "../common/signpost.hpp"
#include "../common/string_util.hpp"
#include "../input/modifier.hpp"

namespace {

os_log_t signpostLog() {
    static os_log_t log = os_log_create("dev.smhkd", "PointsOfInterest");
    return log;
}

}  // namespace

void HotkeyEngine::applyConfig(std::map<Hotkey, std::string> hotkeys, std::vector<RemapBinding> remaps, ConfigProperties config) {
    hotkeys_ = std::move(hotkeys);
    remaps_ = std::move(remaps);
    config_ = std::move(config);
    reset();
}

bool HotkeyEngine::handleEvent(const Chord& current, CGEventType type, bool isRepeat) {
    os_log_t log = signpostLog();
    os_signpost_id_t spid = SIGNPOST_GENERATE(log);
    SIGNPOST_BEGIN(log, spid, "handleEvent");

    if (!config_.blacklist.empty()) {
        os_signpost_id_t bp = SIGNPOST_GENERATE(log);
        SIGNPOST_BEGIN(log, bp, "frontProcessLookup");
        const auto front = getFrontProcessName();
        SIGNPOST_END(log, bp, "frontProcessLookup");
        if (isBlacklisted(front)) {
            clearSequence();
            lastChord_ = std::nullopt;
            SIGNPOST_END(log, spid, "handleEvent", "path=blacklisted");
            return false;
        }
    }

    if (type == kCGEventKeyUp && lastChord_ && lastChord_->keysym.keycode == current.keysym.keycode) {
        lastChord_ = std::nullopt;
    }

    if (type == kCGEventKeyDown && !isRepeat && handleSequence(current)) {
        SIGNPOST_END(log, spid, "handleEvent", "path=sequence");
        return true;
    }

    if (applyRemap(current, type)) {
        SIGNPOST_END(log, spid, "handleEvent", "path=remap");
        return true;
    }

    os_signpost_id_t mp = SIGNPOST_GENERATE(log);
    SIGNPOST_BEGIN(log, mp, "hotkeyMatch", "count=%zu", hotkeys_.size());
    for (const auto& [hotkey, command] : hotkeys_) {
        if (hotkey.chords.size() > 1) continue;
        if (!hotkey.chords[0].isActivatedBy(current)) continue;

        const bool typeMatches =
            (!hotkey.on_release && type == kCGEventKeyDown) || (hotkey.on_release && type == kCGEventKeyUp);
        const bool repeatMatches = (isRepeat && hotkey.repeat) || !isRepeat;
        if (!typeMatches || !repeatMatches) continue;

        debug("hotkey matched: {}", hotkey);
        if (!command.empty()) {
            debug("executing command: {}", command);
            os_signpost_id_t cp = SIGNPOST_GENERATE(log);
            SIGNPOST_BEGIN(log, cp, "executeCommand");
            executeCommand(command);
            SIGNPOST_END(log, cp, "executeCommand");
            if (type == kCGEventKeyDown) {
                lastChord_ = current;
            }
        }
        SIGNPOST_END(log, mp, "hotkeyMatch", "matched=1");
        SIGNPOST_END(log, spid, "handleEvent", "path=hotkey");
        return !hotkey.passthrough;
    }
    SIGNPOST_END(log, mp, "hotkeyMatch", "matched=0");
    SIGNPOST_END(log, spid, "handleEvent", "path=none");
    return false;
}

bool HotkeyEngine::applyRemap(const Chord& chord, CGEventType type) {
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return false;
    }

    for (const auto& remap : remaps_) {
        if (remap.source.chords.size() != 1) {
            continue;
        }
        if (!remap.source.chords[0].isActivatedBy(chord)) {
            continue;
        }
        postKeyEvent(remap.target, type == kCGEventKeyDown);
        return true;
    }
    return false;
}

void HotkeyEngine::synthesizeKeyPress(const Chord& target) {
    postKeyEvent(target, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    postKeyEvent(target, false);
}

void HotkeyEngine::reset() {
    clearSequence();
    lastChord_ = std::nullopt;
}

void HotkeyEngine::clearSequence() {
    sequence_.clear();
    lastPressTime_ = std::chrono::time_point<std::chrono::system_clock>::min();
}

bool HotkeyEngine::handleSequence(const Chord& chord) {
    const auto now = std::chrono::system_clock::now();
    if (lastPressTime_ != std::chrono::time_point<std::chrono::system_clock>::min() && now - lastPressTime_ > config_.maxChordInterval) {
        clearSequence();
        return false;
    }
    lastPressTime_ = now;
    sequence_.push_back(chord);

    for (const auto& [hotkey, command] : hotkeys_) {
        if (hotkey.chords.size() == 1) continue;
        if (sequence_.size() > hotkey.chords.size()) continue;

        bool matches = true;
        for (size_t i = 0; i < sequence_.size(); i++) {
            if (!hotkey.chords[i].isActivatedBy(sequence_[i])) {
                matches = false;
                break;
            }
        }
        if (!matches) continue;

        if (sequence_.size() == hotkey.chords.size()) {
            debug("Matched complete chord sequence ending with: {}", hotkey);
            if (!command.empty()) {
                debug("executing command: {}", command);
                executeCommand(command);
            }
            clearSequence();
            return true;
        }
        debug("Matched partial chord sequence: {}", chord);
        return true;
    }

    clearSequence();
    return false;
}

bool HotkeyEngine::isBlacklisted(std::string_view processName) const {
    if (config_.blacklist.empty()) return false;
    if (processName.empty()) return false;
    return std::ranges::contains(config_.blacklist, processName);
}

void HotkeyEngine::postKeyEvent(const Chord& target, bool keyDown) {
    CGEventRef event = CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(target.keysym.keycode), keyDown);
    if (!event) {
        warn("failed to create synthetic key event for remap");
        return;
    }
    CGEventSetFlags(event, hotkeyFlagsToEventFlags(target.modifiers));
    CGEventSetIntegerValueField(event, kCGEventSourceUserData, SYNTHETIC_REMAP_TAG);
    CGEventPost(kCGSessionEventTap, event);
    CFRelease(event);
}
