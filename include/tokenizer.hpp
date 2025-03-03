#pragma once

#include <string>
#include <utility>

#include "token.hpp"

class Tokenizer {
   private:
    std::string contents;
    size_t position{};
    int row{};
    int col{};

    bool peeked{};
    Token nextToken_;

    bool nextTokenIsCommand{};

   public:
    explicit Tokenizer(std::string contents) : contents(std::move(contents)) {}

    // peek the next token without consuming
    [[nodiscard]] const Token& peek();

    // get the next token and consume it
    Token next();

    [[nodiscard]] bool hasMoreTokens(int offset = 0);

   private:
    // actual implementation of next()
    Token getNextToken();

    [[nodiscard]] std::string readHex();

    // read the rest of the line as a single token
    [[nodiscard]] Token readCommandToken();

    [[nodiscard]] std::string readEventType();

    [[nodiscard]] bool isIdentifierChar(char c);

    // read until whitespace, newline, plus, colon, '#'
    [[nodiscard]] std::string readIdentifier();

    void skipWhitespaceAndComments();

    void skipWhitespace();

    void eatComment();

    void advance();

    void advanceNewline();

    [[nodiscard]] char peekChar(int offset = 0);
};
