#pragma once

#include <string>
#include <utility>

#include "locale.hpp"
#include "token.hpp"

class Tokenizer {
   private:
    // contents of the file
    std::string contents;

    // current position in the file
    size_t position{};

    // current row in the file
    int row{};

    // current column in the file
    int col{};

    // whether we've peeked at the next token or not
    bool peeked{};

    // the next token
    Token nextToken_;

    // whether the next token is a command
    bool nextTokenIsCommand{};

   public:
    explicit Tokenizer(std::string contents) : contents(std::move(contents)) {
        initializeKeycodeMap();
    }

    // peek the next token without consuming
    [[nodiscard]] const Token& peek();

    // get the next token and consume it
    Token next();

    // check if there are more tokens
    [[nodiscard]] bool hasMoreTokens(int offset = 0);

   private:
    // actual implementation of next()
    Token getNextToken();

    [[nodiscard]] std::string readHex();

    // read the rest of the line as a single token
    [[nodiscard]] Token readCommandToken();

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
