#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../input/chord.hpp"
#include "../input/hotkey.hpp"
#include "ast.hpp"

struct ConfigProperties {
    // max time between chord presses
    std::chrono::milliseconds maxChordInterval{3000};

    // minimum time for a keysym to be held to be considered as a hold_modifier
    // otherwise it's considered a normal keysym event
    std::chrono::milliseconds holdModifierThreshold{500};

    // max time between keysyms to be considered as simultaneous
    std::chrono::milliseconds simultaneousThreshold{50};

    // process names to ignore (case-insensitive)
    std::vector<std::string> blacklist;

    // command run with the active chord sequence appended as args whenever
    // it changes (entering/extending/exiting a multi-chord sequence)
    std::string sequenceCommand;
};

struct InterpreterError {
    std::string message;
};

struct RemapBinding {
    Hotkey source;
    Chord target;

    RemapBinding(Hotkey source, Chord target)
        : source(std::move(source)), target(target) {}
};

struct InterpreterResult {
    std::map<Hotkey, std::string> hotkeys;
    std::vector<RemapBinding> remaps;
    ConfigProperties config;
    std::vector<InterpreterError> errors;
};

struct ChordResult {
    std::optional<Chord> chord;
    std::vector<InterpreterError> errors;
};

[[nodiscard]] InterpreterResult interpretProgram(const ast::Program& p);

[[nodiscard]] ChordResult interpretChord(const ast::Chord& ch);
