#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
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

class Interpreter {
   public:
    Interpreter() = default;
    [[nodiscard]] InterpreterResult interpret(const ast::Program& program);
    std::optional<Chord> interpretChordSyntax(const ast::ChordSyntax& syntax);
    [[nodiscard]] const std::vector<InterpreterError>& errors() const { return errors_; }

   private:
    std::unordered_map<std::string, std::vector<ast::ModifierAtom>> defines;
    std::unordered_map<std::string, int> cache;
    std::vector<InterpreterError> errors_;

    void addError(std::string message);

    // modifier resolution
    std::optional<int> resolveModifierFlags(const std::string& name);
    std::optional<int> resolveModifierAtoms(const std::vector<ast::ModifierAtom>& atoms);

    // hotkey building
    void setChordKeyFromAtom(Chord& chord, const ast::KeyAtom& atom);
    std::optional<Hotkey> buildBaseHotkey(const ast::HotkeySyntax& syn);
    std::optional<Chord> buildChord(const ast::ChordSyntax& syntax);
    bool setHotkeyKeys(Hotkey& hk, const ast::HotkeySyntax& syn, std::optional<size_t> braceChordIndex, size_t braceItemIndex);

    // command parsing
    static std::string trim(std::string_view s);
    static std::string unescapeDoubleBraces(std::string_view s);
    std::vector<std::string> parseCommandBraceExpansion(const std::string& command);

    // statement application
    void applyDefine(const ast::DefineModifierStmt& node);
    void applyConfig(const ast::ConfigPropertyStmt& node, ConfigProperties& config);
    void applyRemap(const ast::RemapStmt& node, std::vector<RemapBinding>& remaps);
    void applyHotkey(const ast::HotkeyStmt& h, std::map<Hotkey, std::string>& hotkeys);
};
