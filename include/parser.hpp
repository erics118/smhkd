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
    explicit Parser(Tokenizer tokenizer) : tokenizer(std::move(tokenizer)) {}

    std::unordered_map<Hotkey, std::string> parseFile();

   private:
    std::unordered_map<std::string, std::vector<std::string>> customModifiers;

    void parseDefineModifier();

    int getImplicitFlags(const std::string& literal);

    KeyEventType parseEventType(const std::string& text);

    int getBuiltinModifierFlag(const std::string& mod);

    int getCustomModifierFlag(const std::string& mod, int row, int col);

    int getModifierFlag(const std::string& mod, int row, int col);

    bool isModifier(const std::string& mod);

    // Parse a brace expansion like {1,2,3} into a vector of strings
    std::vector<std::string> parseKeyBraceExpansion();

    // Expand command string with brace expansion
    std::vector<std::string> expandCommandString(const std::string& command);

    // Create a hotkey for each item in the expansion
    std::vector<Hotkey> expandHotkey(const Hotkey& base, const std::vector<std::string>& items, const std::vector<std::string>& expandedCommands);

    // Parse a hotkey that may contain brace expansions
    std::vector<Hotkey> parseHotkeyWithExpansion();
};
