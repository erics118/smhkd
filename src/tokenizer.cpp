#include "tokenizer.hpp"

#include <algorithm>
#include <string>

#include "hotkey.hpp"
#include "token.hpp"

// peek the next token without consuming
const Token& Tokenizer::peek() {
    if (!peeked) {
        nextToken_ = getNextToken();
        peeked = true;
    }
    return nextToken_;
}

// get the next token and consume it
// public facing function that handles peeking, while getNextToken does the actual work
Token Tokenizer::next() {
    Token token = peeked ? nextToken_ : getNextToken();
    // debug("token: {}", token);
    peeked = false;
    return token;
}

bool Tokenizer::hasMoreTokens(int offset) {
    if (peeked && nextToken_.type == TokenType::EndOfFile) {
        return false;
    }
    return (position + offset < contents.size());
}

// TOOD: require pls sign
// TODO: more error handling
// NOLINTNEXTLINE(misc-no-recursion)
Token Tokenizer::getNextToken() {
    skipWhitespaceAndComments();

    // If at EOF
    if (position >= contents.size()) {
        return Token{TokenType::EndOfFile, "", row, col};
    }

    // If next token must be a Command
    if (nextTokenIsCommand) {
        nextTokenIsCommand = false;
        return readCommandToken();
    }

    char c = peekChar();
    int startRow = row;
    int startCol = col;

    // single-character tokens
    if (c == '+') {
        advance();
        return Token{TokenType::Plus, "+", startRow, startCol};
    }

    if (c == '=') {
        advance();
        return Token{TokenType::Equals, "=", startRow, startCol};
    }

    if (c == ':') {
        advance();
        // next token is a command, taking the entire line
        nextTokenIsCommand = true;
        return Token{TokenType::Colon, ":", startRow, startCol};
    }

    if (c == '^') {
        advance();
        return Token{TokenType::Caret, "^", startRow, startCol};
    }

    if (c == '@') {
        advance();
        return Token{TokenType::At, "@", startRow, startCol};
    }

    if (c == '&') {
        advance();
        return Token{TokenType::Ampersand, "&", startRow, startCol};
    }

    if (c == '{') {
        advance();
        return Token{TokenType::OpenBrace, "{", startRow, startCol};
    }

    if (c == '}') {
        advance();
        return Token{TokenType::CloseBrace, "}", startRow, startCol};
    }

    if (c == ',') {
        advance();
        return Token{TokenType::Comma, ",", startRow, startCol};
    }

    if (c == ';') {
        advance();
        return Token{TokenType::Semicolon, ";", startRow, startCol};
    }

    if (c == '0' && peekChar(1) == 'x') {
        std::string hex = readHex();
        return Token{TokenType::KeyHex, hex, startRow, startCol};
    }

    // otherwise, read until delimiter
    std::string text = readIdentifier();
    if (text.empty()) {
        // newline or comment, so get next token
        return getNextToken();
    }

    // check if literal
    if (std::ranges::contains(literal_keycode_str, text)) {
        return Token{TokenType::Literal, text, startRow, startCol};
    }

    // special case: "define_modifier"
    if (text == "define_modifier") {
        return Token{TokenType::DefineModifier, text, startRow, startCol};
    }

    // if single char, key
    if (text.size() == 1) {
        return Token{TokenType::Key, text, startRow, startCol};
    }

    // otherwise, modifier
    return Token{TokenType::Modifier, text, startRow, startCol};
}

std::string Tokenizer::readHex() {
    std::string result;
    // consume the 0x
    advance();
    advance();

    while (hasMoreTokens()) {
        char c = peekChar();
        if (!std::isxdigit(c)) {
            break;
        }
        result.push_back(c);
        advance();
    }
    return result;
}

// read the rest of the line as a single token
Token Tokenizer::readCommandToken() {
    int startRow = row;
    int startCol = col;
    std::string line;
    while (position < contents.size()) {
        char c = peekChar();
        if (c == '\n') {
            break;
        }
        line.push_back(c);
        advance();
    }

    // consume newline
    if (peekChar() == '\n') {
        advanceNewline();
    }

    return Token{TokenType::Command, line, startRow, startCol};
}

bool Tokenizer::isIdentifierChar(char c) {
    return std::isalnum(c) || c == '_';
}

// read until whitespace, newline, plus, colon, '#'
std::string Tokenizer::readIdentifier() {
    std::string result;
    while (hasMoreTokens()) {
        char c = peekChar();
        if (!isIdentifierChar(c)) {
            break;
        }
        result.push_back(c);
        advance();
    }
    return result;
}

// skip whitespace and comments
void Tokenizer::skipWhitespaceAndComments() {
    while (hasMoreTokens()) {
        skipWhitespace();
        if (peekChar() == '#') {
            eatComment();
        } else {
            break;
        }
    }
}

void Tokenizer::skipWhitespace() {
    while (hasMoreTokens()) {
        char c = peekChar();
        if (c == '\n') {
            advanceNewline();
        } else if (std::isspace(c)) {
            advance();
        } else {
            break;
        }
    }
}

// TODO: link comment to hotkey
void Tokenizer::eatComment() {
    while (hasMoreTokens()) {
        if (peekChar() == '\n') {
            advanceNewline();
            return;
        }
        advance();
    }
}

void Tokenizer::advance() {
    if (hasMoreTokens()) {
        position++;
        col++;
    }
}

void Tokenizer::advanceNewline() {
    position++;
    row++;
    col = 0;
}

char Tokenizer::peekChar(int offset) {
    if (!hasMoreTokens()) return '\0';
    return contents[position + offset];
}
