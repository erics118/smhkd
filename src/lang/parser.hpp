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
    std::string contents_;
    Tokenizer tokenizer;
    std::vector<ParseError> errors_;

   public:
    explicit Parser(std::string contents) : contents_(std::move(contents)), tokenizer(contents_) {}
    ast::Program parseProgram();
    std::optional<ast::ChordSyntax> parseChord(const ChordParseOptions& options = {});
    std::optional<std::vector<ast::ChordSyntax>> parseChordSequence(const ChordParseOptions& options = {});
    [[nodiscard]] const std::vector<ParseError>& errors() const { return errors_; }

   private:
    [[nodiscard]] static bool isKeyToken(TokenType type);
    [[nodiscard]] static bool isFlagToken(TokenType type);
    [[nodiscard]] static bool startsChord(const Token& tk);

    std::optional<Token> expect(TokenType expected, std::string_view context);

    void addError(const Token& token, std::string message);
    void addUnexpectedTokenError(const Token& token, std::string_view context, std::string_view expected = {});
    void addUnexpectedEofError(const Token& token, std::string_view context, std::string_view expected = {});
    void skipRemainingTokensOnRow(int row);
    void dropTrailingTokens(int row, std::string_view context);

    std::optional<ast::DefineModifierStmt> parseDefineModifierStmt();
    std::optional<ast::ConfigPropertyStmt> parseConfigPropertyStmt();
    std::optional<ast::ConfigPropertyStmt> parseBlacklistConfigStmt(const Token& cpToken);
    std::optional<ast::ConfigPropertyStmt> parseIntegerConfigStmt(const Token& cpToken);
    std::optional<ast::KeySyntax> parseKeyBraceExpansion();
    [[nodiscard]] std::optional<ast::KeySyntax> parseSingleKeySyntax(const Token& tk);
    std::optional<ast::ChordSyntax> parseChordSyntax(int row, const ChordParseOptions& options);
    std::optional<ast::ChordSyntax> parseSequenceElement(const ChordParseOptions& options);
    std::optional<bool> consumeSequenceSeparator(int row);
    std::optional<ast::Stmt> parseBindingStmt();
    std::optional<ast::HotkeyStmt> parseHotkeyStmt(ast::HotkeySyntax syntax);
    std::optional<ast::RemapStmt> parseRemapStmt(ast::HotkeySyntax syntax);
};
