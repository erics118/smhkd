#pragma once

#include <format>
#include <string>

enum class TokenType {
    DefineModifier,  // 'define_modifier'
    Modifier,        // built-in or user-defined
    Key,             // single character
    KeyHex,          // hex keycode
    Plus,            // '+'
    Equals,          // '='
    Colon,           // ':'
    Command,         // entire line after ':'
    EventType,       // event type '~up', '~down', '~both'
    Literal,         // literal, ie enter, space tab
    Identifier,      // identifier
    At,              // passthrough '@'
    Repeat,          // repeat '&'
    EndOfFile,       // end of file
    OpenBrace,       // '{'
    CloseBrace,      // '}'
    Comma,           // ','
};

template <>
struct std::formatter<TokenType> : std::formatter<std::string_view> {
    auto format(TokenType tt, std::format_context& ctx) const {
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
            case TokenType::EventType: name = "EventType"; break;
            case TokenType::Literal: name = "Literal"; break;
            case TokenType::Identifier: name = "Identifier"; break;
            case TokenType::At: name = "At"; break;
            case TokenType::Repeat: name = "Repeat"; break;
            case TokenType::EndOfFile: name = "EndOfFile"; break;
            case TokenType::OpenBrace: name = "OpenBrace"; break;
            case TokenType::CloseBrace: name = "CloseBrace"; break;
            case TokenType::Comma: name = "Comma"; break;
            default: name = "Unknown";
        }
        return std::format_to(ctx.out(), "{}", name);
    }
};

struct Token {
    TokenType type;
    std::string text;
    int row;
    int col;
};

template <>
struct std::formatter<Token> : std::formatter<std::string_view> {
    auto format(const Token& t, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "'{}' ({}, {}, {})", t.text, t.type, t.row, t.col);
    }
};
