#include "hotkey_engine.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>

#include "../common/command.hpp"
#include "../common/log.hpp"
#include "../input/modifier.hpp"

void HotkeyEngine::applyConfig(std::map<Hotkey, std::string> hotkeys, std::vector<RemapBinding> remaps, ConfigProperties config) {
    hotkeys_ = std::move(hotkeys);
    remaps_ = std::move(remaps);
    config_ = std::move(config);
    reset();
}

bool HotkeyEngine::handleEvent(const Chord& current, CGEventType type, bool isRepeat, std::string_view frontProcessName) {
    if (!config_.blacklist.empty() && isBlacklistedProcess(frontProcessName)) {
        clearSequence();
        lastTriggeredChord_ = std::nullopt;
        return false;
    }

    if (type == kCGEventKeyUp && lastTriggeredChord_ && lastTriggeredChord_->keysym.keycode == current.keysym.keycode) {
        lastTriggeredChord_ = std::nullopt;
    }

    if (type == kCGEventKeyDown && !isRepeat && checkAndExecuteSequence(current)) {
        return true;
    }

    if (checkAndApplyRemap(current, type)) {
        return true;
    }

    for (const auto& [hotkey, command] : hotkeys_) {
        if (hotkey.chords.size() > 1) continue;
        if (!hotkey.chords[0].isActivatedBy(current)) continue;

        const bool eventTypeMatches =
            (!hotkey.on_release && type == kCGEventKeyDown) || (hotkey.on_release && type == kCGEventKeyUp);
        const bool repeatMatches = (isRepeat && hotkey.repeat) || !isRepeat;
        if (!eventTypeMatches || !repeatMatches) continue;

        debug("hotkey matched: {}", hotkey);
        if (!command.empty()) {
            debug("executing command: {}", command);
            executeCommand(command);
            if (type == kCGEventKeyDown) {
                lastTriggeredChord_ = current;
            }
        }
        return !hotkey.passthrough;
    }
    return false;
}

bool HotkeyEngine::checkAndApplyRemap(const Chord& current, CGEventType type) {
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return false;
    }

    for (const auto& remap : remaps_) {
        if (remap.source.chords.size() != 1) {
            continue;
        }
        if (!remap.source.chords[0].isActivatedBy(current)) {
            continue;
        }
        postRemappedKeyEvent(remap.target, type == kCGEventKeyDown);
        return true;
    }
    return false;
}

void HotkeyEngine::synthesizeKeyPress(const Chord& target) {
    postRemappedKeyEvent(target, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    postRemappedKeyEvent(target, false);
}

void HotkeyEngine::reset() {
    clearSequence();
    lastTriggeredChord_ = std::nullopt;
}

void HotkeyEngine::clearSequence() {
    currentChords_.clear();
    lastKeyPressTime_ = std::chrono::time_point<std::chrono::system_clock>::min();
}

bool HotkeyEngine::checkAndExecuteSequence(const Chord& current) {
    const auto now = std::chrono::system_clock::now();
    if (lastKeyPressTime_ != std::chrono::time_point<std::chrono::system_clock>::min() && now - lastKeyPressTime_ > config_.maxChordInterval) {
        clearSequence();
        return false;
    }
    lastKeyPressTime_ = now;
    currentChords_.push_back(current);

    for (const auto& [hotkey, command] : hotkeys_) {
        if (hotkey.chords.size() == 1) continue;
        if (currentChords_.size() > hotkey.chords.size()) continue;

        bool matches = true;
        for (size_t i = 0; i < currentChords_.size(); i++) {
            if (!hotkey.chords[i].isActivatedBy(currentChords_[i])) {
                matches = false;
                break;
            }
        }
        if (!matches) continue;

        if (currentChords_.size() == hotkey.chords.size()) {
            debug("Matched complete chord sequence ending with: {}", hotkey);
            if (!command.empty()) {
                debug("executing command: {}", command);
                executeCommand(command);
            }
            clearSequence();
            return true;
        }
        debug("Matched partial chord sequence: {}", current);
        return true;
    }

    clearSequence();
    return false;
}

bool HotkeyEngine::isBlacklistedProcess(std::string_view frontProcessName) const {
    if (frontProcessName.empty()) return false;
    const auto lowerName = toLower(frontProcessName);
    for (const auto& blocked : config_.blacklist) {
        if (toLower(blocked) == lowerName) {
            return true;
        }
    }
    return false;
}

std::string HotkeyEngine::toLower(std::string_view s) {
    std::string lowered(s);
    std::ranges::transform(lowered, lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}

void HotkeyEngine::postRemappedKeyEvent(const Chord& target, bool keyDown) {
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
