#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "config_properties.hpp"
#include "hotkey.hpp"
#include "tokenizer.hpp"

class Parser {
   private:
    Tokenizer tokenizer;

    ConfigProperties config;

   public:
    explicit Parser(const std::string& contents) : tokenizer(contents) {}

    // parse a file and return a map of hotkeys to commands
    std::map<Hotkey, std::string> parseFile();

    // get the max chord interval
    [[nodiscard]] ConfigProperties getConfigProperties() const { return config; }

   private:
    // map of custom modifier names to their flags
    std::map<std::string, std::vector<std::string>> customModifiers;

    // parse 'define_modifier'
    void parseDefineModifier();

    // parse a config property
    void parseConfigProperty();

    // get the builtin modifier flag for a given modifier
    int getBuiltinModifierFlag(const std::string& mod);

    // get the custom modifier flag for a given modifier
    int getCustomModifierFlag(const std::string& mod, int row, int col);

    // get the modifier flag for a given modifier
    int getModifierFlag(const std::string& mod, int row, int col);

    // check if a given string is a modifier
    bool isModifier(const std::string& mod);

    // parse brace expansion '{1,2,3}' into a vector of strings
    std::vector<Token> parseKeyBraceExpansion();

    // parse simultaneous keys '[a,b]' into a vector of strings
    std::vector<Token> parseSimultaneousKeys();

    // expand command string with brace expansion
    std::vector<std::string> expandCommandString(const std::string& command);

    // Parse a hotkey that may contain brace expansions
    std::vector<std::pair<Hotkey, std::string>> parseHotkeyWithExpansion();
};
