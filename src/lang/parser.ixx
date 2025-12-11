module;

#include <format>
#include <string>
#include <utility>
#include <vector>

export module parser;

import ast;
import token;
import tokenizer;
import keysym;
import log;
import modifier;

export class Parser {
   private:
    Tokenizer tokenizer;

   public:
    explicit Parser(const std::string& contents) : tokenizer(contents) {}
    ast::Program parseProgram();

   private:
    Token consume(TokenType expected);
    ast::DefineModifierStmt parseDefineModifierStmt();
    ast::ConfigPropertyStmt parseConfigPropertyStmt();
    ast::KeySyntax parseKeyBraceExpansionSyntax();
    ast::HotkeyStmt parseHotkeyStmt();
};

ast::Program Parser::parseProgram() {
    ast::Program program;
    while (tokenizer.hasMoreTokens()) {
        const Token tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }
        if (tk.type == TokenType::DefineModifier) {
            auto stmt = parseDefineModifierStmt();
            // only add non-empty statements
            if (!stmt.name.empty() && !stmt.parts.empty()) {
                program.statements.emplace_back(std::move(stmt));
            }
        } else if (tk.type == TokenType::ConfigProperty) {
            auto stmt = parseConfigPropertyStmt();
            // only add non-empty statements
            if (!stmt.name.empty()) {
                program.statements.emplace_back(std::move(stmt));
            }
        } else {
            auto stmt = parseHotkeyStmt();
            // only add non-empty statements
            if (!stmt.syntax.chords.empty() && !stmt.command.empty()) {
                program.statements.emplace_back(std::move(stmt));
            }
        }
    }
    return program;
}

Token Parser::consume(TokenType expected) {
    Token token = tokenizer.next();
    if (token.type != expected) {
        warn("parse error at line {}, column {}: expected {} but got {} ('{}')",
            token.row, token.col, expected, token.type, token.text);
        // continue parsing though
    }
    return token;
}

ast::DefineModifierStmt Parser::parseDefineModifierStmt() {
    debug("Parsing define_modifier");
    consume(TokenType::DefineModifier);
    Token nameToken = tokenizer.next();
    if (nameToken.type != TokenType::Modifier && nameToken.type != TokenType::Key) {
        warn(
            "parse error at line {}, column {}: expected custom modifier name after 'define_modifier' but got {} ('{}')",
            nameToken.row, nameToken.col, nameToken.type, nameToken.text);
        // skip this definition
        return ast::DefineModifierStmt{};
    }
    std::string customName = nameToken.text;
    const Token eqToken = consume(TokenType::Equals);
    ast::DefineModifierStmt stmt;
    stmt.name = customName;
    const int currentRow = eqToken.row;
    while (true) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) break;
        if (tk.row != currentRow) break;
        if (tk.type == TokenType::Plus) {
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::Modifier || tk.type == TokenType::Key) {
            if (auto bi = parseBuiltinModifier(tk.text)) {
                stmt.parts.push_back(ast::ModifierAtom{*bi});
            } else {
                stmt.parts.push_back(ast::ModifierAtom{tk.text});
            }
            tokenizer.next();
            continue;
        }
        break;
    }
    if (stmt.parts.empty()) {
        warn(
            "parse error at line {}: No expansions found for custom modifier '{}'. ", nameToken.row, customName);
        // return empty statement
    }
    return stmt;
}

ast::ConfigPropertyStmt Parser::parseConfigPropertyStmt() {
    debug("Parsing a config property");
    Token cpToken = consume(TokenType::ConfigProperty);
    consume(TokenType::Equals);
    // we don't have an int token, so it's either a key or a modifier
    Token intToken = tokenizer.next();
    if (intToken.type != TokenType::Modifier && intToken.type != TokenType::Key) {
        warn(
            "parse error at line {}, column {}: expected integer after '=' for config property '{}' but got {} ('{}')",
            intToken.row, intToken.col, cpToken.text, intToken.type, intToken.text);
        // return empty statement
        return ast::ConfigPropertyStmt{};
    }
    ast::ConfigPropertyStmt stmt;
    stmt.name = cpToken.text;
    stmt.value = std::stoi(intToken.text);
    return stmt;
}

ast::KeySyntax Parser::parseKeyBraceExpansionSyntax() {
    ast::KeySyntax ks;
    ks.isBraceExpansion = true;
    consume(TokenType::OpenBrace);
    while (true) {
        Token tk = tokenizer.next();
        if (tk.type == TokenType::Key || tk.type == TokenType::Literal) {
            ast::KeyAtom atom;
            if (tk.type == TokenType::Literal) {
                if (auto lit = tryParseLiteralKey(tk.text)) atom.value = *lit;
                else atom.value = ast::KeyChar{tk.text.empty() ? '\0' : tk.text.at(0), false};
            } else {
                atom.value = ast::KeyChar{tk.text.at(0), false};
            }
            ks.items.push_back(atom);
            tk = tokenizer.next();
            if (tk.type == TokenType::Comma) {
                continue;
            }
            if (tk.type == TokenType::CloseBrace) {
                break;
            }
            warn(
                "parse error at line {}, column {}: unexpected token {} ('{}') in brace expansion. Expected ',' or '}}'",
                tk.row, tk.col, tk.type, tk.text);
            // break to end brace expansion
            break;
        }
    }
    return ks;
}

ast::HotkeyStmt Parser::parseHotkeyStmt() {
    ast::HotkeyStmt stmt;
    ast::HotkeySyntax syntax;
    syntax.chords.emplace_back();
    bool foundColon = false;
    while (true) {
        while (true) {
            const Token& tk = tokenizer.peek();
            if (tk.type == TokenType::EndOfFile) {
                warn(
                    "parse error at line {}, column {}: unexpected end of file while "
                    "parsing hotkey. Expected ':' followed by a command",
                    tk.row, tk.col);
                // return empty hotkey statement
                return ast::HotkeyStmt{};
            }
            if (tk.type == TokenType::Colon) {
                consume(TokenType::Colon);
                foundColon = true;
                break;
            }
            if (tk.type == TokenType::Semicolon) {
                consume(TokenType::Semicolon);
                syntax.chords.emplace_back();
                break;
            }
            if (tk.type == TokenType::Plus) {
                consume(TokenType::Plus);
                continue;
            }
            if (tk.type == TokenType::At) {
                syntax.passthrough = true;
                consume(TokenType::At);
                continue;
            }
            if (tk.type == TokenType::Ampersand) {
                syntax.repeat = true;
                consume(TokenType::Ampersand);
                continue;
            }
            if (tk.type == TokenType::Caret) {
                syntax.onRelease = true;
                consume(TokenType::Caret);
                continue;
            }
            if (tk.type == TokenType::Modifier) {
                if (auto bi = parseBuiltinModifier(tk.text)) {
                    syntax.chords.back().modifiers.push_back(ast::ModifierAtom{*bi});
                } else {
                    syntax.chords.back().modifiers.push_back(ast::ModifierAtom{tk.text});
                }
                consume(TokenType::Modifier);
                continue;
            }
            if (tk.type == TokenType::OpenBrace) {
                syntax.chords.back().key = parseKeyBraceExpansionSyntax();
                continue;
            }
            if (tk.type == TokenType::Literal || tk.type == TokenType::Key || tk.type == TokenType::KeyHex) {
                ast::KeySyntax ks;
                ks.isBraceExpansion = false;
                ast::KeyAtom atom;
                if (tk.type == TokenType::Literal) {
                    if (auto lit = tryParseLiteralKey(tk.text)) atom.value = *lit;
                    else atom.value = ast::KeyChar{tk.text.empty() ? '\0' : tk.text.at(0), false};
                } else if (tk.type == TokenType::Key) {
                    atom.value = ast::KeyChar{tk.text.at(0), false};
                } else {
                    try {
                        const uint64_t v = std::stoul(tk.text, nullptr, 16);
                        atom.value = ast::KeyChar{static_cast<char>(static_cast<unsigned char>(v & 0xFF)), true};
                    } catch (...) {
                        atom.value = ast::KeyChar{'\0', true};
                    }
                }
                ks.items.push_back(atom);
                syntax.chords.back().key = ks;
                consume(tk.type);
                continue;
            }
            break;
        }
        if (foundColon) {
            break;
        }
    }
    const Token nextTk = consume(TokenType::Command);
    stmt.syntax = std::move(syntax);
    stmt.command = nextTk.text;
    return stmt;
}
