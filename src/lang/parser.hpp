#pragma once

#include "ast.hpp"
#include "token.hpp"
#include "tokenizer.hpp"
#include "../input/keysym.hpp"
#include "../input/log.hpp"
#include "../input/modifier.hpp"
#include <format>
#include <string>
#include <utility>
#include <vector>



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
