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
    std::optional<ast::Chord> parseChord(const ChordParseOptions& options = {});
    std::optional<std::vector<ast::Chord>> parseChordSequence(const ChordParseOptions& options = {});
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

    std::optional<ast::DefineModifier> parseDefineModifierStmt();
    std::optional<ast::ConfigProperty> parseConfigPropertyStmt();
    std::optional<ast::ConfigProperty> parseBlacklistConfigStmt(const Token& cpToken);
    std::optional<ast::ConfigProperty> parseIntegerConfigStmt(const Token& cpToken);
    std::optional<ast::ConfigProperty> parseStringConfigStmt(const Token& cpToken);
    std::optional<ast::Keysym> parseBraceExpansionKeysym();
    [[nodiscard]] std::optional<ast::SimpleKeysym> consumeSimpleKeysym();
    std::optional<ast::Chord> parseChord(int row, const ChordParseOptions& options);
    std::optional<ast::Chord> parseSequenceElement(const ChordParseOptions& options);
    std::optional<bool> consumeSequenceSeparator(int row);
    std::optional<ast::Stmt> parseBindingStmt();
    std::optional<ast::Hotkey> parseHotkeyStmt(ast::Chords binding);
    std::optional<ast::Remap> parseRemapStmt(ast::Chords binding);
};
