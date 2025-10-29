#pragma once

#include <string>

#include "ast.hpp"
#include "tokenizer.hpp"

class Parser {
   private:
    Tokenizer tokenizer;

   public:
    explicit Parser(const std::string& contents) : tokenizer(contents) {}

    // Parse a file and return an AST Program
    Program parseProgram();

   private:
    // parse 'define_modifier'
    DefineModifierStmt parseDefineModifierStmt();

    // parse a config property
    ConfigPropertyStmt parseConfigPropertyStmt();

    // parse a keysym brace expansion into KeySyntax
    KeySyntax parseKeyBraceExpansionSyntax();

    // parse a hotkey statement (no expansion or resolution here)
    HotkeyStmt parseHotkeyStmt();
};
