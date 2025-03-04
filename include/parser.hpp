#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hotkey.hpp"
#include "tokenizer.hpp"

class Parser {
   private:
    Tokenizer tokenizer;

   public:
    explicit Parser(const std::string& contents) : tokenizer(contents) {}

    // parse a file and return a map of hotkeys to commands
    std::unordered_map<Hotkey, std::string> parseFile();

   private:
    // map of custom modifier names to their flags
    std::unordered_map<std::string, std::vector<std::string>> customModifiers;

    // parse 'define_modifier'
    void parseDefineModifier();

    // get the implicit flags for a given literal
    int getImplicitFlags(const std::string& literal);

    // parse an event type
    KeyEventType parseEventType(const std::string& text);

    // get the builtin modifier flag for a given modifier
    int getBuiltinModifierFlag(const std::string& mod);

    // get the custom modifier flag for a given modifier
    int getCustomModifierFlag(const std::string& mod, int row, int col);

    // get the modifier flag for a given modifier
    int getModifierFlag(const std::string& mod, int row, int col);

    // check if a given string is a modifier
    bool isModifier(const std::string& mod);

    // parse brace expansion '{1,2,3}' into a vector of strings
    std::vector<std::string> parseKeyBraceExpansion();

    // expand command string with brace expansion
    std::vector<std::string> expandCommandString(const std::string& command);

    // create a hotkey for each item in the expansion
    std::vector<Hotkey> expandHotkey(const Hotkey& base, const std::vector<std::string>& items, const std::vector<std::string>& expandedCommands);

    // Parse a hotkey that may contain brace expansions
    std::vector<Hotkey> parseHotkeyWithExpansion();
};
