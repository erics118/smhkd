#include "parser.hpp"

#include "../common/log.hpp"

ast::Program Parser::parseProgram() {
    ast::Program program;
    while (tokenizer.hasMoreTokens()) {
        const Token tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }
        if (tk.type == TokenType::DefineModifier) {
            if (auto stmt = parseDefineModifierStmt()) {
                program.statements.emplace_back(std::move(*stmt));
            }
        } else if (tk.type == TokenType::ConfigProperty) {
            if (auto stmt = parseConfigPropertyStmt()) {
                program.statements.emplace_back(std::move(*stmt));
            }
        } else {
            if (auto stmt = parseHotkeyStmt()) {
                program.statements.emplace_back(std::move(*stmt));
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
    }
    return token;
}

std::optional<ast::DefineModifierStmt> Parser::parseDefineModifierStmt() {
    debug("Parsing define_modifier");
    consume(TokenType::DefineModifier);
    Token nameToken = tokenizer.next();
    if (nameToken.type != TokenType::Modifier && nameToken.type != TokenType::Key) {
        warn(
            "parse error at line {}, column {}: expected custom modifier name after 'define_modifier' but got {} ('{}')",
            nameToken.row, nameToken.col, nameToken.type, nameToken.text);
        return std::nullopt;
    }
    std::string customName = nameToken.text;
    const Token eqToken = consume(TokenType::Equals);
    if (eqToken.type != TokenType::Equals) {
        return std::nullopt;
    }
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
        return std::nullopt;
    }
    return stmt;
}

std::optional<ast::ConfigPropertyStmt> Parser::parseConfigPropertyStmt() {
    debug("Parsing a config property");
    Token cpToken = consume(TokenType::ConfigProperty);
    ast::ConfigPropertyStmt stmt;
    stmt.name = cpToken.text;

    if (cpToken.text == "blacklist") {
        if (tokenizer.peek().type != TokenType::Equals) {
            warn(
                "parse error at line {}, column {}: expected '=' after blacklist config but got {} ('{}')",
                tokenizer.peek().row, tokenizer.peek().col, tokenizer.peek().type, tokenizer.peek().text);
            return std::nullopt;
        }
        consume(TokenType::Equals);
        if (tokenizer.peek().type != TokenType::OpenBracket) {
            warn(
                "parse error at line {}, column {}: expected '[' after blacklist config but got {} ('{}')",
                tokenizer.peek().row, tokenizer.peek().col, tokenizer.peek().type, tokenizer.peek().text);
            return std::nullopt;
        }
        consume(TokenType::OpenBracket);
        while (tokenizer.hasMoreTokens()) {
            const Token& tk = tokenizer.peek();
            if (tk.type == TokenType::CloseBracket) {
                consume(TokenType::CloseBracket);
                break;
            }
            if (tk.type == TokenType::EndOfFile) {
                warn("parse error at line {}, column {}: unexpected end of file while parsing blacklist", tk.row, tk.col);
                break;
            }
            if (tk.type == TokenType::Comma) {
                consume(TokenType::Comma);
                continue;
            }
            if (tk.type == TokenType::String || tk.type == TokenType::Modifier || tk.type == TokenType::Key) {
                Token valueToken = tokenizer.next();
                if (!valueToken.text.empty()) {
                    stmt.listValues.push_back(valueToken.text);
                }
                continue;
            }
            warn(
                "parse error at line {}, column {}: unexpected token {} ('{}') in blacklist. Expected quoted string or identifier",
                tk.row, tk.col, tk.type, tk.text);
            tokenizer.next();
        }
        if (stmt.listValues.empty()) {
            warn("blacklist config provided but no process names were parsed");
            return std::nullopt;
        }
        return stmt;
    }

    const Token eqToken = consume(TokenType::Equals);
    if (eqToken.type != TokenType::Equals) {
        return std::nullopt;
    }
    Token intToken = tokenizer.next();
    if (intToken.type != TokenType::Modifier && intToken.type != TokenType::Key) {
        warn(
            "parse error at line {}, column {}: expected integer after '=' for config property '{}' but got {} ('{}')",
            intToken.row, intToken.col, cpToken.text, intToken.type, intToken.text);
        return std::nullopt;
    }
    try {
        stmt.intValue = std::stoi(intToken.text);
    } catch (...) {
        warn(
            "parse error at line {}, column {}: value '{}' for config property '{}' is not a valid integer",
            intToken.row, intToken.col, intToken.text, cpToken.text);
        return std::nullopt;
    }
    return stmt;
}

std::optional<ast::KeySyntax> Parser::parseKeyBraceExpansionSyntax() {
    ast::KeySyntax ks;
    ks.isBraceExpansion = true;
    const Token openToken = consume(TokenType::OpenBrace);
    if (openToken.type != TokenType::OpenBrace) {
        return std::nullopt;
    }

    while (true) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            warn(
                "parse error at line {}, column {}: unexpected end of file while parsing brace expansion",
                tk.row, tk.col);
            return std::nullopt;
        }
        if (tk.type == TokenType::CloseBrace) {
            if (ks.items.empty()) {
                warn(
                    "parse error at line {}, column {}: brace expansion must contain at least one key",
                    tk.row, tk.col);
                return std::nullopt;
            }
            consume(TokenType::CloseBrace);
            return ks;
        }

        auto keySyntax = parseSingleKeySyntax(tk);
        if (!keySyntax) {
            warn(
                "parse error at line {}, column {}: unexpected token {} ('{}') in brace expansion",
                tk.row, tk.col, tk.type, tk.text);
            tokenizer.next();
            return std::nullopt;
        }
        ks.items.push_back(keySyntax->items.front());
        consume(tk.type);

        const Token& separator = tokenizer.peek();
        if (separator.type == TokenType::Comma) {
            consume(TokenType::Comma);
            continue;
        }
        if (separator.type == TokenType::CloseBrace) {
            continue;
        }
        warn(
            "parse error at line {}, column {}: unexpected token {} ('{}') in brace expansion. Expected ',' or '}}'",
            separator.row, separator.col, separator.type, separator.text);
        tokenizer.next();
        return std::nullopt;
    }
}

std::optional<ast::KeySyntax> Parser::parseSingleKeySyntax(const Token& tk) const {
    if (tk.type != TokenType::Literal && tk.type != TokenType::Key && tk.type != TokenType::KeyHex) {
        return std::nullopt;
    }

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
    return ks;
}

bool Parser::parseHotkeyToken(ast::HotkeySyntax& syntax, bool& foundColon) {
    const Token& tk = tokenizer.peek();
    if (tk.type == TokenType::EndOfFile) {
        warn(
            "parse error at line {}, column {}: unexpected end of file while "
            "parsing hotkey. Expected ':' followed by a command",
            tk.row, tk.col);
        return false;
    }
    if (tk.type == TokenType::Colon) {
        consume(TokenType::Colon);
        foundColon = true;
        return true;
    }
    if (tk.type == TokenType::Semicolon) {
        consume(TokenType::Semicolon);
        syntax.chords.emplace_back();
        return true;
    }
    if (tk.type == TokenType::Plus) {
        consume(TokenType::Plus);
        return true;
    }
    if (tk.type == TokenType::At) {
        syntax.passthrough = true;
        consume(TokenType::At);
        return true;
    }
    if (tk.type == TokenType::Ampersand) {
        syntax.repeat = true;
        consume(TokenType::Ampersand);
        return true;
    }
    if (tk.type == TokenType::Caret) {
        syntax.onRelease = true;
        consume(TokenType::Caret);
        return true;
    }
    if (tk.type == TokenType::Modifier) {
        if (auto bi = parseBuiltinModifier(tk.text)) {
            syntax.chords.back().modifiers.push_back(ast::ModifierAtom{*bi});
        } else {
            syntax.chords.back().modifiers.push_back(ast::ModifierAtom{tk.text});
        }
        consume(TokenType::Modifier);
        return true;
    }
    if (tk.type == TokenType::OpenBrace) {
        auto keySyntax = parseKeyBraceExpansionSyntax();
        if (!keySyntax) {
            return false;
        }
        syntax.chords.back().key = std::move(*keySyntax);
        return true;
    }
    if (auto keySyntax = parseSingleKeySyntax(tk)) {
        syntax.chords.back().key = std::move(*keySyntax);
        consume(tk.type);
        return true;
    }

    warn(
        "parse error at line {}, column {}: unexpected token {} ('{}') while parsing hotkey",
        tk.row, tk.col, tk.type, tk.text);
    tokenizer.next();
    return true;
}

std::optional<ast::HotkeyStmt> Parser::parseHotkeyStmt() {
    ast::HotkeyStmt stmt;
    ast::HotkeySyntax syntax;
    syntax.chords.emplace_back();
    bool foundColon = false;
    while (!foundColon) {
        if (!parseHotkeyToken(syntax, foundColon)) {
            return std::nullopt;
        }
    }
    const Token nextTk = consume(TokenType::Command);
    if (nextTk.type != TokenType::Command || nextTk.text.empty()) {
        warn(
            "parse error at line {}, column {}: expected command after ':' but got {} ('{}')",
            nextTk.row, nextTk.col, nextTk.type, nextTk.text);
        return std::nullopt;
    }
    stmt.syntax = std::move(syntax);
    stmt.command = nextTk.text;
    return stmt;
}
