#pragma once
#include <string>
#include <utility>

#include "log.hpp"
#include "token.hpp"

class Tokenizer {
   public:
    explicit Tokenizer(std::string contents) : m_contents(std::move(contents)) {}

    // Peek the next token without consuming
    const Token& peekToken() {
        if (!m_peeked) {
            m_nextToken = getNextToken();
            m_peeked = true;
        }
        return m_nextToken;
    }

    // Get the next token and consume it
    Token nextToken() {
        Token token = m_peeked ? m_nextToken : getNextToken();
        debug("next token: {}", token);
        m_peeked = false;
        return token;
    }

    bool hasMoreTokens() {
        if (m_peeked && m_nextToken.type == TokenType::EndOfFile) {
            return false;
        }
        skipWhitespaceAndComments();
        return (m_position < m_contents.size());
    }

   private:
    std::string m_contents;
    size_t m_position{};
    int m_row{};
    int m_col{};

    bool m_peeked{};
    Token m_nextToken;

    bool m_nextTokenIsCommand{};

    Token getNextToken() {
        skipWhitespaceAndComments();

        // If at EOF
        if (m_position >= m_contents.size()) {
            return Token{TokenType::EndOfFile, "", m_row, m_col};
        }

        // If next token must be a Command
        if (m_nextTokenIsCommand) {
            m_nextTokenIsCommand = false;
            return readCommandToken();
        }

        char c = peekChar();
        int startRow = m_row;
        int startCol = m_col;

        // Single-character tokens
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
            // Next token is entire line => Command
            m_nextTokenIsCommand = true;
            return Token{TokenType::Colon, ":", startRow, startCol};
        }
        if (c == '~') {
            std::string et = readEventType();
            return Token{TokenType::EventType, et, startRow, startCol};
        }

        // Otherwise, read text until a delimiter
        std::string text = readUntilDelimiter();
        if (text.empty()) {
            // Possibly a newline or comment – skip and re-fetch
            return getNextToken();
        }

        // Special check: "define_modifier"
        if (text == "define_modifier") {
            return Token{TokenType::DefineModifier, text, startRow, startCol};
        }

        // If single char => Key
        if (text.size() == 1) {
            return Token{TokenType::Key, text, startRow, startCol};
        }

        // Otherwise => treat as "Modifier" for now (could be built-in or custom)
        return Token{TokenType::Modifier, text, startRow, startCol};
    }

    // read the rest of the line as a single token
    Token readCommandToken() {
        int startRow = m_row;
        int startCol = m_col;
        std::string line;
        while (m_position < m_contents.size()) {
            char c = peekChar();
            if (c == '\n') {
                break;
            }
            line.push_back(c);
            advance();
        }
        // consume newline if present
        if (m_position < m_contents.size() && peekChar() == '\n') {
            advanceNewline();
        }
        return Token{TokenType::Command, line, startRow, startCol};
    }

    std::string readEventType() {
        std::string result;
        while (m_position < m_contents.size()) {
            char c = peekChar();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n'
                || c == '+' || c == ':' || c == '#') {
                break;
            }
            result.push_back(c);
            advance();
        }
        return result;  // e.g. "~up"
    }

    // read until whitespace, newline, plus, colon, '#'
    std::string readUntilDelimiter() {
        std::string result;
        while (m_position < m_contents.size()) {
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
    void skipWhitespaceAndComments() {
        while (true) {
            skipWhitespace();
            if (m_position < m_contents.size() && peekChar() == '#') {
                eatComment();
            } else {
                break;
            }
        }
    }

    void skipWhitespace() {
        while (m_position < m_contents.size()) {
            char c = m_contents[m_position];
            if (c == ' ' || c == '\t' || c == '\r') {
                advance();
            } else if (c == '\n') {
                advanceNewline();
            } else {
                break;
            }
        }
    }

    void eatComment() {
        while (m_position < m_contents.size()) {
            if (peekChar() == '\n') {
                advanceNewline();
                return;
            }
            advance();
        }
    }

    void advance() {
        if (m_position < m_contents.size()) {
            m_position++;
            m_col++;
        }
    }

    void advanceNewline() {
        m_position++;
        m_row++;
        m_col = 0;
    }

    char peekChar() const {
        if (m_position >= m_contents.size()) return '\0';
        return m_contents[m_position];
    }
};
