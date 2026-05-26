#pragma once

#include <string>

#include "token.hpp"

class Tokenizer {
   private:
    std::string_view contents;
    size_t position{};
    int row{};
    int col{};
    bool peeked{};
    Token nextToken_;
    bool nextTokenIsCommand{};

   public:
    explicit Tokenizer(std::string_view contents) : contents(contents) {}

    [[nodiscard]] const Token& peek();
    Token next();
    [[nodiscard]] bool hasMoreTokens(int offset = 0);

   private:
    Token getNextToken();
    [[nodiscard]] std::string readHex();
    [[nodiscard]] std::string readQuotedString();
    [[nodiscard]] Token readCommandToken();
    [[nodiscard]] bool isIdentifierChar(char c);
    [[nodiscard]] std::string readIdentifier();
    void skipWhitespaceAndComments();
    void skipWhitespace();
    void eatComment();
    void advance();
    void advanceNewline();
    [[nodiscard]] char peekChar(int offset = 0);
};
