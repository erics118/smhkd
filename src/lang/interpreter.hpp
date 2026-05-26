#pragma once

#include <chrono>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "../common/log.hpp"
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

struct InterpreterResult {
    std::map<Hotkey, std::string> hotkeys;
    ConfigProperties config;
};

class Interpreter {
   public:
    Interpreter() = default;
    [[nodiscard]] InterpreterResult interpret(const ast::Program& program);

   private:
    std::unordered_map<std::string, std::vector<ast::ModifierAtom>> defines;
    std::unordered_map<std::string, int> cache;

    // modifier resolution
    int resolveModifierFlags(const std::string& name);

    // hotkey building
    void setChordKeyFromAtom(Chord& chord, const ast::KeyAtom& atom);
    Hotkey buildBaseHotkey(const ast::HotkeySyntax& syn);
    void setHotkeyKeys(Hotkey& hk, const ast::HotkeySyntax& syn, int braceChordIndex, size_t braceItemIndex);

    // command parsing
    static std::string trim(std::string_view s);
    static std::string unescapeDoubleBraces(std::string_view s);
    std::vector<std::string> parseCommandBraceExpansion(const std::string& command);
};
