#include "tokenizer.hpp"

#include <string>

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
    peeked = false;
    return token;
}

bool Tokenizer::hasMoreTokens() {
    if (peeked && nextToken_.type == TokenType::EndOfFile) {
        return false;
    }
    skipWhitespaceAndComments();
    return (position < contents.size());
}

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

    if (c == '~') {
        std::string et = readEventType();
        return Token{TokenType::EventType, et, startRow, startCol};
    }

    if (c == '@') {
        advance();
        return Token{TokenType::At, "@", startRow, startCol};
    }

    if (c == '&') {
        advance();
        return Token{TokenType::Repeat, "&", startRow, startCol};
    }

    // otherwise, read until delimiter
    std::string text = readUntilDelimiter();
    if (text.empty()) {
        // newline or comment, so get next token
        return getNextToken();
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

    // consume newline if present
    if (position < contents.size() && peekChar() == '\n') {
        advanceNewline();
    }

    return Token{TokenType::Command, line, startRow, startCol};
}

std::string Tokenizer::readEventType() {
    std::string result;
    while (position < contents.size()) {
        char c = peekChar();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n'
            || c == '+' || c == ':' || c == '#') {
            break;
        }
        result.push_back(c);
        advance();
    }
    return result;  // e.g. '~up'
}

// read until whitespace, newline, plus, colon, '#'
std::string Tokenizer::readUntilDelimiter() {
    std::string result;
    while (position < contents.size()) {
        char c = peekChar();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n'
            || c == '+' || c == ':' || c == '#' || c == '=') {
            break;
        }
        result.push_back(c);
        advance();
    }
    return result;
}

// skip whitespace and comments
void Tokenizer::skipWhitespaceAndComments() {
    while (true) {
        skipWhitespace();
        if (position < contents.size() && peekChar() == '#') {
            eatComment();
        } else {
            break;
        }
    }
}

void Tokenizer::skipWhitespace() {
    while (position < contents.size()) {
        char c = contents[position];
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            advanceNewline();
        } else {
            break;
        }
    }
}

void Tokenizer::eatComment() {
    while (position < contents.size()) {
        if (peekChar() == '\n') {
            advanceNewline();
            return;
        }
        advance();
    }
}

void Tokenizer::advance() {
    if (position < contents.size()) {
        position++;
        col++;
    }
}

void Tokenizer::advanceNewline() {
    position++;
    row++;
    col = 0;
}

char Tokenizer::peekChar() const {
    if (position >= contents.size()) return '\0';
    return contents[position];
}
