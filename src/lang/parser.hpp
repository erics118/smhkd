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

struct ChordParseOptions {
    bool allowBraceExpansion{false};
};

class Parser {
   private:
    Tokenizer tokenizer;
    std::vector<ParseError> errors_;

   public:
    explicit Parser(const std::string& contents) : tokenizer(contents) {}
    ast::Program parseProgram();
    std::optional<ast::ChordSyntax> parseChord(const ChordParseOptions& options = {});
    std::optional<std::vector<ast::ChordSyntax>> parseChordSequence(const ChordParseOptions& options = {});
    [[nodiscard]] const std::vector<ParseError>& errors() const { return errors_; }

   private:
    Token consume(TokenType expected);
    void addError(const Token& token, std::string message);
    std::optional<ast::DefineModifierStmt> parseDefineModifierStmt();
    std::optional<ast::ConfigPropertyStmt> parseConfigPropertyStmt();
    std::optional<ast::KeySyntax> parseKeyBraceExpansionSyntax();
    [[nodiscard]] std::optional<ast::KeySyntax> parseSingleKeySyntax(const Token& tk) const;
    [[nodiscard]] static bool startsChord(const Token& tk);
    std::optional<ast::ChordSyntax> parseChordSyntax(int row, const ChordParseOptions& options);
    std::optional<ast::ChordSyntax> parseRemapTargetChord(int row);
    std::optional<ast::Stmt> parseBindingStmt();
    std::optional<ast::HotkeyStmt> parseHotkeyStmt(ast::HotkeySyntax syntax);
    std::optional<ast::RemapStmt> parseRemapStmt(ast::HotkeySyntax syntax);
};
