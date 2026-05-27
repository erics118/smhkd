#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ast.hpp"
#include "token.hpp"
#include "tokenizer.hpp"

struct ParseError {
    int row;
    int col;
    std::string message;
};

class Parser {
   private:
    Tokenizer tokenizer;
    std::vector<ParseError> errors_;

   public:
    explicit Parser(const std::string& contents) : tokenizer(contents) {}
    ast::Program parseProgram();
    [[nodiscard]] const std::vector<ParseError>& errors() const { return errors_; }

   private:
    Token consume(TokenType expected);
    void addError(const Token& token, std::string message);
    std::optional<ast::DefineModifierStmt> parseDefineModifierStmt();
    std::optional<ast::ConfigPropertyStmt> parseConfigPropertyStmt();
    std::optional<ast::KeySyntax> parseKeyBraceExpansionSyntax();
    [[nodiscard]] std::optional<ast::KeySyntax> parseSingleKeySyntax(const Token& tk) const;
    bool parseHotkeyToken(ast::HotkeySyntax& syntax, bool& foundColon);
    std::optional<ast::HotkeyStmt> parseHotkeyStmt();
};
