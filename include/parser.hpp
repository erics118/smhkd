#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hotkey.hpp"
#include "tokenizer.hpp"

class Parser {
   private:
    Tokenizer m_tokenizer;

   public:
    explicit Parser(Tokenizer tokenizer) : m_tokenizer(std::move(tokenizer)) {}

    std::vector<Hotkey> parseFile();

   private:
    std::unordered_map<std::string, std::vector<std::string>> m_customModifiers;

    void parseDefineModifier();

    Hotkey parseHotkey();

    KeyEventType parseEventType(const std::string& text);

    int getBuiltinModifierFlag(const std::string& mod);

    int getCustomModifierFlag(const std::string& mod, int row, int col);

    int getModifierFlag(const std::string& mod, int row, int col);
};
