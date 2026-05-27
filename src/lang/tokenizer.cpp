#include "tokenizer.hpp"

#include <algorithm>
#include <cctype>

#include "../input/keysym.hpp"

const Token& Tokenizer::peek(size_t offset) {
    while (bufferedTokens.size() <= offset) {
        bufferedTokens.push_back(getNextToken());
    }
    return bufferedTokens[offset];
}

Token Tokenizer::next() {
    if (!bufferedTokens.empty()) {
        Token token = bufferedTokens.front();
        bufferedTokens.pop_front();
        return token;
    }
    return getNextToken();
}

bool Tokenizer::hasRemainingInput(int offset) {
    return (position + offset < contents.size());
}

Token Tokenizer::getNextToken() {
    while (true) {
        skipWhitespaceAndComments();
        if (position >= contents.size()) {
            return Token{TokenType::EndOfFile, "", row, col};
        }
        if (nextTokenIsCommand) {
            nextTokenIsCommand = false;
            return readCommandToken();
        }
        char c = peekChar();
        int startRow = row;
        int startCol = col;

        if (c == '+') {
            advance();
            return Token{TokenType::Plus, "+", startRow, startCol};
        }
        if (c == '|') {
            advance();
            return Token{TokenType::Pipe, "|", startRow, startCol};
        }
        if (c == '=') {
            advance();
            return Token{TokenType::Equals, "=", startRow, startCol};
        }
        if (c == ':') {
            advance();
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
        if (c == '[') {
            advance();
            return Token{TokenType::OpenBracket, "[", startRow, startCol};
        }
        if (c == ']') {
            advance();
            return Token{TokenType::CloseBracket, "]", startRow, startCol};
        }
        if (c == '"') {
            std::string value = readQuotedString();
            return Token{TokenType::String, value, startRow, startCol};
        }

        const std::string text = readIdentifier();
        if (text.empty()) {
            const std::string invalid(1, c);
            advance();
            return Token{TokenType::Invalid, invalid, startRow, startCol};
        }

        if (tryParseLiteralKey(text).has_value()) {
            return Token{TokenType::Literal, text, startRow, startCol};
        }
        if (text == "define_modifier") {
            return Token{TokenType::DefineModifier, text, startRow, startCol};
        }
        if (text == "max_chord_interval" || text == "hold_modifier_threshold" || text == "simultaneous_threshold" || text == "blacklist") {
            return Token{TokenType::ConfigProperty, text, startRow, startCol};
        }
        if (std::ranges::all_of(text, [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
            return Token{TokenType::Integer, text, startRow, startCol};
        }
        if (text.size() == 1) {
            return Token{TokenType::Key, text, startRow, startCol};
        }
        return Token{TokenType::Modifier, text, startRow, startCol};
    }
}

std::string Tokenizer::readHex() {
    std::string result;
    advance();
    advance();
    while (hasRemainingInput()) {
        char c = peekChar();
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            break;
        }
        result.push_back(c);
        advance();
    }
    return result;
}

std::string Tokenizer::readQuotedString() {
    std::string result;
    advance();
    while (hasRemainingInput()) {
        char c = peekChar();
        if (c == '\\' && hasRemainingInput(1) && peekChar(1) == '"') {
            result.push_back('"');
            advance();
            advance();
            continue;
        }
        if (c == '"') {
            advance();
            break;
        }
        if (c == '\n') {
            break;
        }
        result.push_back(c);
        advance();
    }
    return result;
}

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
    if (peekChar() == '\n') {
        advanceNewline();
    }
    return Token{TokenType::Command, line, startRow, startCol};
}

bool Tokenizer::isIdentifierChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string Tokenizer::readIdentifier() {
    std::string result;
    while (hasRemainingInput()) {
        char c = peekChar();
        if (!isIdentifierChar(c)) {
            break;
        }
        result.push_back(c);
        advance();
    }
    return result;
}

void Tokenizer::skipWhitespaceAndComments() {
    while (hasRemainingInput()) {
        skipWhitespace();
        if (peekChar() == '#') {
            eatComment();
        } else {
            break;
        }
    }
}

void Tokenizer::skipWhitespace() {
    while (hasRemainingInput()) {
        char c = peekChar();
        if (c == '\n') {
            advanceNewline();
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else {
            break;
        }
    }
}

void Tokenizer::eatComment() {
    while (hasRemainingInput()) {
        if (peekChar() == '\n') {
            advanceNewline();
            return;
        }
        advance();
    }
}

void Tokenizer::advance() {
    if (hasRemainingInput()) {
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
    if (!hasRemainingInput(offset)) return '\0';
    return contents[position + offset];
}
