#pragma once

#include <string>
#include <utility>

#include "token.hpp"

class Tokenizer {
   public:
    explicit Tokenizer(std::string contents) : m_contents(std::move(contents)) {}

    // Peek the next token without consuming
    const Token& peekToken();

    // Get the next token and consume it
    Token nextToken();

    bool hasMoreTokens();

   private:
    std::string m_contents;
    size_t m_position{};
    int m_row{};
    int m_col{};

    bool m_peeked{};
    Token m_nextToken;

    bool m_nextTokenIsCommand{};

    Token getNextToken();

    // read the rest of the line as a single token
    Token readCommandToken();

    std::string readEventType();

    // read until whitespace, newline, plus, colon, '#'
    std::string readUntilDelimiter();

    // skip whitespace and comments
    void skipWhitespaceAndComments();

    void skipWhitespace();

    void eatComment();

    void advance();

    void advanceNewline();

    [[nodiscard]] char peekChar() const;
};
