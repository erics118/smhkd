#include "parser.hpp"

ast::Program Parser::parseProgram() {
    ast::Program program;
    while (tokenizer.hasMoreTokens()) {
        const Token tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }
        const int startRow = tk.row;
        const int startCol = tk.col;
        bool parsed = false;
        if (tk.type == TokenType::DefineModifier) {
            if (auto stmt = parseDefineModifierStmt()) {
                program.statements.emplace_back(std::move(*stmt));
                parsed = true;
            }
        } else if (tk.type == TokenType::ConfigProperty) {
            if (auto stmt = parseConfigPropertyStmt()) {
                program.statements.emplace_back(std::move(*stmt));
                parsed = true;
            }
        } else {
            if (auto stmt = parseBindingStmt()) {
                program.statements.emplace_back(std::move(*stmt));
                parsed = true;
            }
        }
        // if a previous function failed without consuming any token, drop
        // a token to guarantee moving forward
        if (!parsed) {
            const Token& after = tokenizer.peek();
            if (after.type != TokenType::EndOfFile && after.row == startRow && after.col == startCol) {
                tokenizer.next();
            }
        }
    }
    return program;
}

void Parser::addError(const Token& token, std::string message) {
    errors_.push_back(ParseError{
        .row = token.row,
        .col = token.col,
        .message = std::move(message),
    });
}

Token Parser::consume(TokenType expected) {
    Token token = tokenizer.next();
    if (token.type != expected) {
        addError(token, std::format("expected {} but got {} ('{}')", expected, token.type, token.text));
    }
    return token;
}

std::optional<ast::DefineModifierStmt> Parser::parseDefineModifierStmt() {
    consume(TokenType::DefineModifier);
    Token nameToken = tokenizer.next();
    if (nameToken.type != TokenType::Modifier && nameToken.type != TokenType::Key) {
        addError(nameToken, std::format(
                                "expected custom modifier name after 'define_modifier' but got {} ('{}')",
                                nameToken.type,
                                nameToken.text));
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
        addError(nameToken, std::format("no expansions found for custom modifier '{}'", customName));
        return std::nullopt;
    }
    return stmt;
}

std::optional<ast::ConfigPropertyStmt> Parser::parseConfigPropertyStmt() {
    Token cpToken = consume(TokenType::ConfigProperty);
    if (cpToken.text == "blacklist") {
        return parseBlacklistConfigStmt(cpToken);
    }
    return parseScalarConfigStmt(cpToken);
}

std::optional<ast::ConfigPropertyStmt> Parser::parseBlacklistConfigStmt(const Token& cpToken) {
    ast::ConfigPropertyStmt stmt;
    stmt.name = cpToken.text;

    if (tokenizer.peek().type != TokenType::Equals) {
        addError(tokenizer.peek(), std::format(
                                       "expected '=' after blacklist config but got {} ('{}')",
                                       tokenizer.peek().type,
                                       tokenizer.peek().text));
        return std::nullopt;
    }
    consume(TokenType::Equals);
    if (tokenizer.peek().type != TokenType::OpenBracket) {
        addError(tokenizer.peek(), std::format(
                                       "expected '[' after blacklist config but got {} ('{}')",
                                       tokenizer.peek().type,
                                       tokenizer.peek().text));
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
            addError(tk, "unexpected end of file while parsing blacklist");
            break;
        }
        if (tk.type == TokenType::Comma) {
            consume(TokenType::Comma);
            continue;
        }
        if (tk.type == TokenType::String || tk.type == TokenType::Modifier || tk.type == TokenType::Key || tk.type == TokenType::Integer) {
            Token valueToken = tokenizer.next();
            if (!valueToken.text.empty()) {
                stmt.listValues.push_back(valueToken.text);
            }
            continue;
        }
        addError(tk, std::format(
                         "unexpected token {} ('{}') in blacklist. Expected quoted string or identifier",
                         tk.type,
                         tk.text));
        tokenizer.next();
    }
    if (stmt.listValues.empty()) {
        addError(cpToken, "blacklist config provided but no process names were parsed");
        return std::nullopt;
    }
    return stmt;
}

std::optional<ast::ConfigPropertyStmt> Parser::parseScalarConfigStmt(const Token& cpToken) {
    ast::ConfigPropertyStmt stmt;
    stmt.name = cpToken.text;

    const Token eqToken = consume(TokenType::Equals);
    if (eqToken.type != TokenType::Equals) {
        return std::nullopt;
    }
    Token intToken = tokenizer.next();
    if (intToken.type != TokenType::Integer) {
        addError(intToken, std::format(
                               "expected integer after '=' for config property '{}' but got {} ('{}')",
                               cpToken.text,
                               intToken.type,
                               intToken.text));
        return std::nullopt;
    }
    // tokenizer guarantees all-digit text; only out-of-range can throw
    try {
        stmt.intValue = std::stoi(intToken.text);
    } catch (const std::out_of_range&) {
        addError(intToken, std::format(
                               "value '{}' for config property '{}' is out of range",
                               intToken.text,
                               cpToken.text));
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
            addError(tk, "unexpected end of file while parsing brace expansion");
            return std::nullopt;
        }
        if (tk.type == TokenType::CloseBrace) {
            if (ks.items.empty()) {
                addError(tk, "brace expansion must contain at least one key");
                return std::nullopt;
            }
            consume(TokenType::CloseBrace);
            return ks;
        }

        auto keySyntax = parseSingleKeySyntax(tk);
        if (!keySyntax) {
            addError(tk, std::format(
                             "unexpected token {} ('{}') in brace expansion",
                             tk.type,
                             tk.text));
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
        addError(separator, std::format(
                                "unexpected token {} ('{}') in brace expansion. Expected ',' or '}}'",
                                separator.type,
                                separator.text));
        tokenizer.next();
        return std::nullopt;
    }
}

std::optional<ast::KeySyntax> Parser::parseSingleKeySyntax(const Token& tk) {
    if (tk.type != TokenType::Literal && tk.type != TokenType::Key
        && tk.type != TokenType::KeyHex && tk.type != TokenType::Integer) {
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
    } else if (tk.type == TokenType::Integer) {
        // single-digit Integer doubles as a Key
        // multi-digit integers are not valid keys
        if (tk.text.size() != 1) {
            addError(tk, std::format(
                             "'{}' is not a valid key. Use a single digit, a named literal, or 0xNN for a raw keycode",
                             tk.text));
            return std::nullopt;
        }
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

bool Parser::startsChord(const Token& tk) {
    return tk.type == TokenType::Modifier || tk.type == TokenType::OpenBrace
        || tk.type == TokenType::Literal || tk.type == TokenType::Key || tk.type == TokenType::KeyHex
        || tk.type == TokenType::Integer;
}

std::optional<ast::ChordSyntax> Parser::parseChordSyntax(int row, const ChordParseOptions& options) {
    ast::ChordSyntax chord;
    while (tokenizer.hasMoreTokens()) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile || tk.row != row) {
            break;
        }
        if (tk.type == TokenType::Plus) {
            consume(TokenType::Plus);
            continue;
        }
        if (tk.type == TokenType::Modifier && !chord.key.has_value()) {
            if (auto bi = parseBuiltinModifier(tk.text)) {
                chord.modifiers.push_back(ast::ModifierAtom{*bi});
            } else {
                chord.modifiers.push_back(ast::ModifierAtom{tk.text});
            }
            consume(TokenType::Modifier);
            continue;
        }
        if (tk.type == TokenType::OpenBrace) {
            if (!options.allowBraceExpansion) {
                addError(tk, "brace expansion is not allowed here");
                return std::nullopt;
            }
            if (chord.key.has_value()) {
                addError(tk, std::format("unexpected token {} ('{}') after chord key", tk.type, tk.text));
                return std::nullopt;
            }
            auto keySyntax = parseKeyBraceExpansionSyntax();
            if (!keySyntax) {
                return std::nullopt;
            }
            chord.key = std::move(*keySyntax);
            break;
        }
        if (auto keySyntax = parseSingleKeySyntax(tk)) {
            if (chord.key.has_value()) {
                addError(tk, std::format("unexpected token {} ('{}') after chord key", tk.type, tk.text));
                return std::nullopt;
            }
            chord.key = std::move(*keySyntax);
            consume(tk.type);
            break;
        }
        // parseSingleKeySyntax returned nullopt: either the token isn't a key
        // candidate (then we just stop) or it is one but was rejected with an
        // error added (e.g. multi-digit Integer) — consume to make progress.
        if (tk.type == TokenType::Literal || tk.type == TokenType::Key
            || tk.type == TokenType::KeyHex || tk.type == TokenType::Integer) {
            tokenizer.next();
            return std::nullopt;
        }
        break;
    }

    if (!chord.key.has_value()) {
        const Token& tk = tokenizer.peek();
        addError(tk, "chord is missing a key");
        return std::nullopt;
    }
    return chord;
}

std::optional<ast::ChordSyntax> Parser::parseChord(const ChordParseOptions& options) {
    const Token& tk = tokenizer.peek();
    if (tk.type == TokenType::EndOfFile) {
        addError(tk, "unexpected end of file while parsing chord");
        return std::nullopt;
    }
    auto chord = parseChordSyntax(tk.row, options);
    if (!chord) {
        return std::nullopt;
    }
    while (tokenizer.hasMoreTokens()) {
        const Token& trailing = tokenizer.peek();
        if (trailing.type == TokenType::EndOfFile || trailing.row != tk.row) {
            break;
        }
        addError(trailing, std::format(
                               "unexpected token {} ('{}') after chord",
                               trailing.type,
                               trailing.text));
        tokenizer.next();
    }
    return chord;
}

std::optional<std::vector<ast::ChordSyntax>> Parser::parseChordSequence(const ChordParseOptions& options) {
    std::vector<ast::ChordSyntax> chords;
    bool expectingChord = true;

    while (true) {
        const Token& current = tokenizer.peek();
        if (current.type == TokenType::EndOfFile) {
            if (expectingChord) {
                addError(current, "unexpected end of file while parsing chord sequence");
                return std::nullopt;
            }
            break;
        }
        if (current.type == TokenType::Colon || current.type == TokenType::Pipe) {
            if (expectingChord) {
                addError(current, std::format("expected a chord before '{}'", current.text));
                return std::nullopt;
            }
            break;
        }
        if (current.type == TokenType::Semicolon) {
            if (expectingChord) {
                addError(current, "expected a chord before ';'");
                return std::nullopt;
            }
            consume(TokenType::Semicolon);
            expectingChord = true;
            continue;
        }
        if (!startsChord(current)) {
            addError(current, std::format(
                                  "unexpected token {} ('{}') while parsing chord sequence",
                                  current.type,
                                  current.text));
            return std::nullopt;
        }
        auto chord = parseChordSyntax(current.row, options);
        if (!chord) {
            return std::nullopt;
        }
        chords.push_back(std::move(*chord));
        expectingChord = false;
    }

    if (chords.empty()) {
        const Token& tk = tokenizer.peek();
        addError(tk, "expected at least one chord");
        return std::nullopt;
    }
    return chords;
}

std::optional<ast::Stmt> Parser::parseBindingStmt() {
    ast::HotkeySyntax syntax;
    bool foundColon = false;
    bool foundPipe = false;
    while (true) {
        const Token& current = tokenizer.peek();
        if (current.type == TokenType::EndOfFile) {
            addError(current, "unexpected end of file while parsing hotkey. Expected ':' followed by a command or '|' followed by a remap target");
            return std::nullopt;
        }
        if (current.type == TokenType::At) {
            syntax.passthrough = true;
            consume(TokenType::At);
            continue;
        }
        if (current.type == TokenType::Ampersand) {
            syntax.repeat = true;
            consume(TokenType::Ampersand);
            continue;
        }
        if (current.type == TokenType::Caret) {
            syntax.onRelease = true;
            consume(TokenType::Caret);
            continue;
        }
        break;
    }

    auto chords = parseChordSequence(ChordParseOptions{.allowBraceExpansion = true});
    if (!chords) {
        return std::nullopt;
    }
    syntax.chords = std::move(*chords);

    const Token& delimiter = tokenizer.peek();
    if (delimiter.type == TokenType::Colon) {
        consume(TokenType::Colon);
        foundColon = true;
    } else if (delimiter.type == TokenType::Pipe) {
        foundPipe = true;
    } else {
        addError(delimiter, std::format(
                                "unexpected token {} ('{}') while parsing hotkey. Expected ':' or '|'",
                                delimiter.type,
                                delimiter.text));
        return std::nullopt;
    }
    if (foundPipe) {
        if (auto stmt = parseRemapStmt(std::move(syntax))) {
            return ast::Stmt{std::move(*stmt)};
        }
        return std::nullopt;
    }
    if (auto stmt = parseHotkeyStmt(std::move(syntax))) {
        return ast::Stmt{std::move(*stmt)};
    }
    return std::nullopt;
}

std::optional<ast::HotkeyStmt> Parser::parseHotkeyStmt(ast::HotkeySyntax syntax) {
    ast::HotkeyStmt stmt;
    const Token nextTk = consume(TokenType::Command);
    if (nextTk.type != TokenType::Command || nextTk.text.empty()) {
        addError(nextTk, std::format(
                             "expected command after ':' but got {} ('{}')",
                             nextTk.type,
                             nextTk.text));
        return std::nullopt;
    }
    stmt.syntax = std::move(syntax);
    stmt.command = nextTk.text;
    return stmt;
}

std::optional<ast::ChordSyntax> Parser::parseRemapTargetChord(int row) {
    auto target = parseChordSyntax(row, ChordParseOptions{.allowBraceExpansion = false});
    if (!target) {
        return std::nullopt;
    }
    return target;
}

std::optional<ast::RemapStmt> Parser::parseRemapStmt(ast::HotkeySyntax syntax) {
    ast::RemapStmt stmt;
    stmt.source = std::move(syntax);
    const Token& pipeToken = tokenizer.peek();
    const int row = pipeToken.row;
    consume(TokenType::Pipe);
    auto target = parseRemapTargetChord(row);
    if (!target) {
        return std::nullopt;
    }
    stmt.target = std::move(*target);

    while (tokenizer.hasMoreTokens()) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile || tk.row != row) {
            break;
        }
        addError(tk, std::format(
                         "unexpected token {} ('{}') after remap target",
                         tk.type,
                         tk.text));
        tokenizer.next();
    }
    return stmt;
}
