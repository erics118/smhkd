#pragma once

#include <deque>
#include <string>

#include "token.hpp"

class Tokenizer {
   private:
    std::string_view contents;
    size_t position{};
    int row{};
    int col{};
    std::deque<Token> bufferedTokens;
    bool nextTokenIsCommand{};

   public:
    explicit Tokenizer(std::string_view contents) : contents(contents) {}

    [[nodiscard]] const Token& peek(size_t offset = 0);
    Token next();

   private:
    [[nodiscard]] bool hasRemainingInput(int offset = 0);
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
