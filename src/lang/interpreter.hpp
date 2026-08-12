#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <variant>
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

    // corner size as a percent of each trackpad axis
    int cornerSize{15};

    // max finger-contact time for a corner tap
    std::chrono::milliseconds tapTimeout{300};

    // process names to ignore (case-insensitive)
    std::vector<std::string> blacklist;

    // command run with the active chord sequence appended as args whenever
    // it changes (entering/extending/exiting a multi-chord sequence)
    std::string sequenceCommand;
};

struct InterpreterError {
    std::string message;
};

// a binding's action is either a shell command (hotkey) or a target chord (remap)
using BindingAction = std::variant<std::string, Chord>;

struct Binding {
    Hotkey source;
    BindingAction action;
};

// a corner-tap trigger and its action, dispatched by the touch layer rather than
// the keyboard event tap
struct TapBinding {
    Zone zone;
    ModifierFlags modifiers;
    BindingAction action;
};

struct InterpreterResult {
    std::vector<Binding> bindings;
    std::vector<TapBinding> tapBindings;
    ConfigProperties config;
    std::vector<InterpreterError> errors;
};

struct ChordResult {
    std::optional<Chord> chord;
    std::vector<InterpreterError> errors;
};

[[nodiscard]] InterpreterResult interpretProgram(const ast::Program& p);

[[nodiscard]] ChordResult interpretChord(const ast::Chord& ch);
