module;

#include <format>
#include <string>

export module smhkd.token;

export enum class TokenType {
    DefineModifier,
    Modifier,
    Key,
    KeyHex,
    Plus,
    Equals,
    Colon,
    Command,
    Literal,
    Caret,
    At,
    Ampersand,
    EndOfFile,
    OpenBrace,
    CloseBrace,
    Comma,
    Semicolon,
    ConfigProperty,
};

export template <>
struct std::formatter<TokenType> : std::formatter<std::string_view> {
    auto format(const TokenType& tt, std::format_context& ctx) const {
        std::string_view name;
        switch (tt) {
            case TokenType::DefineModifier: name = "DefineModifier"; break;
            case TokenType::Modifier: name = "Modifier"; break;
            case TokenType::Key: name = "Key"; break;
            case TokenType::KeyHex: name = "KeyHex"; break;
            case TokenType::Plus: name = "Plus"; break;
            case TokenType::Equals: name = "Equals"; break;
            case TokenType::Colon: name = "Colon"; break;
            case TokenType::Command: name = "Command"; break;
            case TokenType::Literal: name = "Literal"; break;
            case TokenType::At: name = "At"; break;
            case TokenType::Ampersand: name = "Repeat"; break;
            case TokenType::EndOfFile: name = "EndOfFile"; break;
            case TokenType::OpenBrace: name = "OpenBrace"; break;
            case TokenType::CloseBrace: name = "CloseBrace"; break;
            case TokenType::Comma: name = "Comma"; break;
            case TokenType::Semicolon: name = "Semicolon"; break;
            case TokenType::ConfigProperty: name = "ConfigProperty"; break;
            default: name = "Unknown";
        }
        return std::format_to(ctx.out(), "{}", name);
    }
};

export struct Token {
    TokenType type;
    std::string text;
    int row;
    int col;
};

export template <>
struct std::formatter<Token> : std::formatter<std::string_view> {
    auto format(const Token& t, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "'{}' ({}, {}, {})", t.text, t.type, t.row, t.col);
    }
};
