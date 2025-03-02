#pragma once

#include <string>
#include <utility>

#include "token.hpp"

class Tokenizer {
   private:
    std::string m_contents;
    size_t m_position{};
    int m_row{};
    int m_col{};

    bool m_peeked{};
    Token m_nextToken;

    bool m_nextTokenIsCommand{};

   public:
    explicit Tokenizer(std::string contents) : m_contents(std::move(contents)) {}

    // Peek the next token without consuming
    [[nodiscard]] const Token& peekToken();

    // Get the next token and consume it
    Token nextToken();

    [[nodiscard]] bool hasMoreTokens();

   private:
    Token getNextToken();

    // read the rest of the line as a single token
    [[nodiscard]] Token readCommandToken();

    [[nodiscard]] std::string readEventType();

    // read until whitespace, newline, plus, colon, '#'
    [[nodiscard]] std::string readUntilDelimiter();

    // skip whitespace and comments
    void skipWhitespaceAndComments();

    void skipWhitespace();

    void eatComment();

    void advance();

    void advanceNewline();

    [[nodiscard]] char peekChar() const;
};
