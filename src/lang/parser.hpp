#pragma once

#include <optional>
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
    std::optional<ast::DefineModifierStmt> parseDefineModifierStmt();
    std::optional<ast::ConfigPropertyStmt> parseConfigPropertyStmt();
    std::optional<ast::KeySyntax> parseKeyBraceExpansionSyntax();
    [[nodiscard]] std::optional<ast::KeySyntax> parseSingleKeySyntax(const Token& token) const;
    bool parseHotkeyToken(ast::HotkeySyntax& syntax, bool& foundColon);
    std::optional<ast::HotkeyStmt> parseHotkeyStmt();
};
