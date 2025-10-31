module;

#include <algorithm>
#include <string>

export module smhkd.tokenizer;
import smhkd.token;
import smhkd.keysym;

export class Tokenizer {
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

const Token& Tokenizer::peek() {
    if (!peeked) {
        nextToken_ = getNextToken();
        peeked = true;
    }
    return nextToken_;
}

Token Tokenizer::next() {
    Token token = peeked ? nextToken_ : getNextToken();
    peeked = false;
    return token;
}

bool Tokenizer::hasMoreTokens(int offset) {
    if (peeked && nextToken_.type == TokenType::EndOfFile) {
        return false;
    }
    return (position + offset < contents.size());
}

Token Tokenizer::getNextToken() {
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

    const std::string text = readIdentifier();
    if (text.empty()) {
        return getNextToken();
    }

    if (std::ranges::contains(literal_keycode_str, text)) {
        return Token{TokenType::Literal, text, startRow, startCol};
    }
    if (text == "define_modifier") {
        return Token{TokenType::DefineModifier, text, startRow, startCol};
    }
    if (text == "max_chord_interval" || text == "hold_modifier_threshold" || text == "simultaneous_threshold") {
        return Token{TokenType::ConfigProperty, text, startRow, startCol};
    }
    if (text.size() == 1) {
        return Token{TokenType::Key, text, startRow, startCol};
    }
    return Token{TokenType::Modifier, text, startRow, startCol};
}

std::string Tokenizer::readHex() {
    std::string result;
    advance();
    advance();
    while (hasMoreTokens()) {
        char c = peekChar();
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
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

bool Tokenizer::isIdentifierChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

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
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else {
            break;
        }
    }
}

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
    if (!hasMoreTokens(offset)) return '\0';
    return contents[position + offset];
}
