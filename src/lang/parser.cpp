#include "parser.hpp"

ast::Program Parser::parseProgram() {
    ast::Program program;
    while (true) {
        const Token tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }
        const int startRow = tk.row;
        bool parsed = false;
        if (tk.type == TokenType::DefineModifier) {
            if (auto stmt = parseDefineModifierStmt()) {
                program.statements.emplace_back(std::move(*stmt));
                parsed = true;
            }
        } else if (tk.type == TokenType::ConfigProperty || startsConfigAssignment()) {
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
        // drop remaining tokens on the failing line, so only a single error is produced, rather than multiple
        if (!parsed) {
            skipRemainingTokensOnRow(startRow);
        }
    }
    return program;
}

bool Parser::startsConfigAssignment() {
    const Token first = tokenizer.peek();
    if (first.type != TokenType::Modifier && first.type != TokenType::Key) {
        return false;
    }

    const Token second = tokenizer.peek(1);
    return second.row == first.row && second.type == TokenType::Equals;
}

void Parser::addError(const Token& token, std::string message) {
    errors_.push_back(ParseError{
        .row = token.row,
        .col = token.col,
        .message = std::move(message),
    });
}

void Parser::addUnexpectedTokenError(const Token& token, std::string_view context, std::string_view expected) {
    addError(token, std::format("unexpected token {} ('{}') {}{}{}",
                        token.type, token.text, context, expected.empty() ? "" : ". Expected ", expected));
}

void Parser::addUnexpectedEofError(const Token& token, std::string_view context, std::string_view expected) {
    addError(token, std::format("unexpected end of file {}{}{}",
                        context, expected.empty() ? "" : ". Expected ", expected));
}

void Parser::skipRemainingTokensOnRow(int row) {
    while (true) {
        const Token after = tokenizer.peek();
        if (after.type == TokenType::EndOfFile || after.row != row) {
            break;
        }
        tokenizer.next();
    }
}

void Parser::dropTrailingTokens(int row, std::string_view context) {
    while (true) {
        const Token trailing = tokenizer.peek();
        if (trailing.type == TokenType::EndOfFile || trailing.row != row) {
            break;
        }
        addUnexpectedTokenError(trailing, context);
        tokenizer.next();
    }
}

std::optional<Token> Parser::expect(TokenType expected, std::string_view context) {
    const Token current = tokenizer.peek();
    if (current.type != expected) {
        addError(current, std::format("expected {} {} but got {} ('{}')",
                              expected, context, current.type, current.text));
        return std::nullopt;
    }
    return tokenizer.next();
}

std::optional<ast::DefineModifierStmt> Parser::parseDefineModifierStmt() {
    tokenizer.next();
    Token nameToken = tokenizer.next();
    if (nameToken.type != TokenType::Modifier && nameToken.type != TokenType::Key) {
        addUnexpectedTokenError(nameToken, "after 'define_modifier'", "custom modifier name");
        return std::nullopt;
    }
    std::string customName = nameToken.text;
    auto eqToken = expect(TokenType::Equals, "after custom modifier name");
    if (!eqToken) {
        return std::nullopt;
    }
    ast::DefineModifierStmt stmt;
    stmt.name = customName;
    const int currentRow = eqToken->row;
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
    Token cpToken = tokenizer.next();
    if (cpToken.text == "blacklist") {
        return parseBlacklistConfigStmt(cpToken);
    }
    return parseIntegerConfigStmt(cpToken);
}

std::optional<ast::ConfigPropertyStmt> Parser::parseBlacklistConfigStmt(const Token& cpToken) {
    ast::ConfigPropertyStmt stmt;
    stmt.name = cpToken.text;

    if (!expect(TokenType::Equals, "after blacklist config")) {
        return std::nullopt;
    }
    if (!expect(TokenType::OpenBracket, "after blacklist config")) {
        return std::nullopt;
    }
    while (true) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::CloseBracket) {
            tokenizer.next();
            break;
        }
        if (tk.type == TokenType::EndOfFile) {
            addUnexpectedEofError(tk, "while parsing blacklist");
            break;
        }
        if (tk.type == TokenType::Comma) {
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::String || tk.type == TokenType::Modifier || tk.type == TokenType::Key || tk.type == TokenType::Integer) {
            Token valueToken = tokenizer.next();
            if (!valueToken.text.empty()) {
                stmt.listValues.push_back(valueToken.text);
            }
            continue;
        }
        addUnexpectedTokenError(tk, "in blacklist", "quoted string or identifier");
        tokenizer.next();
    }
    if (stmt.listValues.empty()) {
        addError(cpToken, "blacklist config provided but no process names were parsed");
        return std::nullopt;
    }
    dropTrailingTokens(cpToken.row, "after blacklist config");
    return stmt;
}

std::optional<ast::ConfigPropertyStmt> Parser::parseIntegerConfigStmt(const Token& cpToken) {
    ast::ConfigPropertyStmt stmt;
    stmt.name = cpToken.text;

    if (!expect(TokenType::Equals, std::format("after config property '{}'", cpToken.text))) {
        return std::nullopt;
    }
    Token intToken = tokenizer.next();
    if (intToken.type != TokenType::Integer) {
        addUnexpectedTokenError(intToken, std::format("after '=' for config property '{}'", cpToken.text), "integer");
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
    dropTrailingTokens(cpToken.row, std::format("after config property '{}'", cpToken.text));
    return stmt;
}

std::optional<ast::KeySyntax> Parser::parseKeyBraceExpansion() {
    ast::KeySyntax ks;
    ks.isBraceExpansion = true;
    if (!expect(TokenType::OpenBrace, "to start brace expansion")) {
        return std::nullopt;
    }

    while (true) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            addUnexpectedEofError(tk, "while parsing brace expansion");
            return std::nullopt;
        }
        if (tk.type == TokenType::CloseBrace) {
            if (ks.items.empty()) {
                addError(tk, "brace expansion must contain at least one key");
                return std::nullopt;
            }
            tokenizer.next();
            return ks;
        }

        auto keySyntax = parseSingleKeySyntax(tk);
        if (!keySyntax) {
            addUnexpectedTokenError(tk, "in brace expansion");
            tokenizer.next();
            return std::nullopt;
        }
        ks.items.push_back(keySyntax->items.front());
        tokenizer.next();

        const Token& separator = tokenizer.peek();
        if (separator.type == TokenType::Comma) {
            tokenizer.next();
            continue;
        }
        if (separator.type == TokenType::CloseBrace) {
            continue;
        }
        addUnexpectedTokenError(separator, "in brace expansion", "',' or '}'");
        tokenizer.next();
        return std::nullopt;
    }
}

std::optional<ast::KeySyntax> Parser::parseSingleKeySyntax(const Token& tk) {
    if (!isKeyToken(tk.type)) {
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
        if (tk.text.empty()) {
            addError(tk, "blank hex literal: expected 0x?? with at least one hex digit");
            return std::nullopt;
        }
        uint64_t v = 0;
        try {
            v = std::stoul(tk.text, nullptr, 16);
        } catch (const std::out_of_range&) {
            addError(tk, std::format("hex keycode '0x{}' is out of range (max 0xFF)", tk.text));
            return std::nullopt;
        }
        if (v > 0xFF) {
            addError(tk, std::format("hex keycode '0x{}' is out of range (max 0xFF)", tk.text));
            return std::nullopt;
        }
        atom.value = ast::KeyChar{static_cast<char>(static_cast<unsigned char>(v)), true};
    }
    ks.items.push_back(atom);
    return ks;
}

bool Parser::isKeyToken(TokenType type) {
    return type == TokenType::Literal || type == TokenType::Key
        || type == TokenType::KeyHex || type == TokenType::Integer;
}

bool Parser::isFlagToken(TokenType type) {
    return type == TokenType::At || type == TokenType::Ampersand || type == TokenType::Caret;
}

bool Parser::startsChord(const Token& tk) {
    return tk.type == TokenType::Modifier || tk.type == TokenType::OpenBrace
        || isKeyToken(tk.type);
}

std::optional<ast::ChordSyntax> Parser::parseChordSyntax(int row, const ChordParseOptions& options) {
    ast::ChordSyntax chord;
    while (true) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile || tk.row != row) {
            break;
        }
        if (tk.type == TokenType::Plus) {
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::Modifier && !chord.key.has_value()) {
            if (auto bi = parseBuiltinModifier(tk.text)) {
                chord.modifiers.push_back(ast::ModifierAtom{*bi});
            } else {
                chord.modifiers.push_back(ast::ModifierAtom{tk.text});
            }
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::OpenBrace) {
            if (!options.allowBraceExpansion) {
                addError(tk, "brace expansion is not allowed here");
                return std::nullopt;
            }
            if (chord.key.has_value()) {
                addUnexpectedTokenError(tk, "after chord key");
                return std::nullopt;
            }
            auto keySyntax = parseKeyBraceExpansion();
            if (!keySyntax) {
                return std::nullopt;
            }
            chord.key = std::move(*keySyntax);
            break;
        }
        if (auto keySyntax = parseSingleKeySyntax(tk)) {
            if (chord.key.has_value()) {
                addUnexpectedTokenError(tk, "after chord key");
                return std::nullopt;
            }
            chord.key = std::move(*keySyntax);
            tokenizer.next();
            break;
        }
        if (tk.type == TokenType::Invalid) {
            addUnexpectedTokenError(tk, "while parsing chord sequence", "one of: Modifier, OpenBrace, Literal, Key, KeyHex, Integer");
            tokenizer.next();
            return std::nullopt;
        }
        // parseSingleKeySyntax returned nullopt: either the token isn't a key
        // candidate (then we just stop) or it is one but was rejected with an
        // error added (e.g. multi-digit Integer) — consume to make progress.
        if (isKeyToken(tk.type)) {
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

std::optional<ast::ChordSyntax> Parser::parseSequenceElement(const ChordParseOptions& options) {
    const Token start = tokenizer.peek();
    if (start.type == TokenType::EndOfFile) {
        addUnexpectedEofError(start, "while parsing chord sequence");
        return std::nullopt;
    }
    if (start.type == TokenType::Colon || start.type == TokenType::Pipe) {
        addUnexpectedTokenError(start, "while parsing chord sequence", "chord before ':' or '|'");
        return std::nullopt;
    }
    if (start.type == TokenType::Semicolon) {
        addUnexpectedTokenError(start, "while parsing chord sequence", "chord before ';'");
        return std::nullopt;
    }
    if (!startsChord(start)) {
        addUnexpectedTokenError(start, "while parsing chord sequence", "one of: Modifier, OpenBrace, Literal, Key, KeyHex, Integer");
        return std::nullopt;
    }
    return parseChordSyntax(start.row, options);
}

std::optional<bool> Parser::consumeSequenceSeparator(int row) {
    const Token separator = tokenizer.peek();
    if (separator.type == TokenType::EndOfFile || separator.type == TokenType::Colon || separator.type == TokenType::Pipe
        || isFlagToken(separator.type)) {
        return false;
    }
    if (separator.row != row) {
        return false;
    }
    if (separator.type != TokenType::Semicolon) {
        addUnexpectedTokenError(separator, "while parsing chord sequence", "';', ':' or '|'");
        return std::nullopt;
    }
    tokenizer.next();
    return true;
}

std::optional<ast::ChordSyntax> Parser::parseChord(const ChordParseOptions& options) {
    const Token tk = tokenizer.peek();
    if (tk.type == TokenType::EndOfFile) {
        addUnexpectedEofError(tk, "while parsing chord");
        return std::nullopt;
    }
    auto chord = parseChordSyntax(tk.row, options);
    if (!chord) {
        return std::nullopt;
    }
    dropTrailingTokens(tk.row, "after chord");
    return chord;
}

std::optional<std::vector<ast::ChordSyntax>> Parser::parseChordSequence(const ChordParseOptions& options) {
    std::vector<ast::ChordSyntax> chords;
    const int row = tokenizer.peek().row;
    while (true) {
        auto chord = parseSequenceElement(options);
        if (!chord) return std::nullopt;
        chords.push_back(std::move(*chord));
        auto sep = consumeSequenceSeparator(row);
        if (!sep) return std::nullopt;
        if (!*sep) return chords;
    }
}

std::optional<ast::Stmt> Parser::parseBindingStmt() {
    ast::HotkeySyntax syntax;
    auto chords = parseChordSequence(ChordParseOptions{.allowBraceExpansion = true});
    if (!chords) {
        return std::nullopt;
    }
    syntax.chords = std::move(*chords);

    while (true) {
        const Token& c = tokenizer.peek();
        if (c.type == TokenType::At) {
            syntax.passthrough = true;
            tokenizer.next();
        } else if (c.type == TokenType::Ampersand) {
            syntax.repeat = true;
            tokenizer.next();
        } else if (c.type == TokenType::Caret) {
            syntax.onRelease = true;
            tokenizer.next();
        } else {
            break;
        }
    }

    const Token& delimiter = tokenizer.peek();
    if (delimiter.type == TokenType::EndOfFile) {
        addUnexpectedEofError(delimiter, "after chord sequence", "':' or '|'");
        return std::nullopt;
    }
    if (delimiter.type == TokenType::Colon) {
        tokenizer.next();
        if (auto stmt = parseHotkeyStmt(std::move(syntax))) {
            return ast::Stmt{std::move(*stmt)};
        }
        return std::nullopt;
    }
    if (delimiter.type == TokenType::Pipe) {
        if (auto stmt = parseRemapStmt(std::move(syntax))) {
            return ast::Stmt{std::move(*stmt)};
        }
        return std::nullopt;
    }
    addUnexpectedTokenError(delimiter, "while parsing hotkey", "':' or '|'");
    return std::nullopt;
}

std::optional<ast::HotkeyStmt> Parser::parseHotkeyStmt(ast::HotkeySyntax syntax) {
    ast::HotkeyStmt stmt;
    auto nextTk = expect(TokenType::Command, "after ':'");
    if (!nextTk) return std::nullopt;
    if (nextTk->text.empty()) {
        addUnexpectedTokenError(*nextTk, "after ':'", "non-empty command");
        return std::nullopt;
    }
    stmt.syntax = std::move(syntax);
    stmt.command = nextTk->text;
    return stmt;
}

std::optional<ast::RemapStmt> Parser::parseRemapStmt(ast::HotkeySyntax syntax) {
    ast::RemapStmt stmt;
    stmt.source = std::move(syntax);
    const int row = tokenizer.peek().row;
    tokenizer.next();
    auto target = parseChordSyntax(row, ChordParseOptions{.allowBraceExpansion = false});
    if (!target) {
        return std::nullopt;
    }
    stmt.target = std::move(*target);
    dropTrailingTokens(row, "after remap target");
    return stmt;
}
