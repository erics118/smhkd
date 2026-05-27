#include "hotkey_engine.hpp"

#include <algorithm>
#include <cctype>

#include "../common/command.hpp"
#include "../common/log.hpp"

void HotkeyEngine::applyConfig(std::map<Hotkey, std::string> hotkeys, ConfigProperties config) {
    hotkeys_ = std::move(hotkeys);
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
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}
