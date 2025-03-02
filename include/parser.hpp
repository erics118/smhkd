#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hotkey.hpp"
#include "log.hpp"
#include "tokenizer.hpp"

class Parser {
   public:
    explicit Parser(Tokenizer tokenizer) : m_tokenizer(std::move(tokenizer)) {}

    std::vector<Hotkey> parseFile();

   private:
    Tokenizer m_tokenizer;

    std::unordered_map<std::string, std::vector<std::string>> m_customModifiers;

    void parseDefineModifier();

    Hotkey parseHotkey();

    KeyEventType parseEventType(const std::string& text);

    // expandModifier: if it's one of the built-ins or the "customModifiers" map
    //    - If "cmd", "ctrl", "alt", or "shift", return that alone.
    //    - Else if we have a user-defined name, expand it to the underlying list
    //      (which may include built-in or *other* custom modifiers).
    //    - Potentially do a recursive expansion if needed (be careful of infinite loops).
    std::vector<std::string> expandModifier(const std::string& mod, int row, int col);
};
