#pragma once

#include <string>

#include "ast.hpp"
#include "token.hpp"
#include "tokenizer.hpp"

class Parser {
   private:
    Tokenizer tokenizer;

   public:
    explicit Parser(const std::string& contents) : tokenizer(contents) {}
    ast::Program parseProgram();

   private:
    Token consume(TokenType expected);
    ast::DefineModifierStmt parseDefineModifierStmt();
    ast::ConfigPropertyStmt parseConfigPropertyStmt();
    ast::KeySyntax parseKeyBraceExpansionSyntax();
    ast::HotkeyStmt parseHotkeyStmt();
};
