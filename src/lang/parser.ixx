module;

#include <format>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module smhkd.parser;
import smhkd.ast;
import smhkd.token;
import smhkd.tokenizer;
import smhkd.keysym;
import smhkd.log;
import smhkd.modifier;

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
        Token tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }
        if (tk.type == TokenType::DefineModifier) {
            program.statements.emplace_back(parseDefineModifierStmt());
        } else if (tk.type == TokenType::ConfigProperty) {
            program.statements.emplace_back(parseConfigPropertyStmt());
        } else {
            program.statements.emplace_back(parseHotkeyStmt());
        }
    }
    return program;
}

Token Parser::consume(TokenType expected) {
    Token token = tokenizer.next();
    if (token.type != expected) {
        throw std::runtime_error(std::format("Expected {} but got {}", expected, token));
    }
    return token;
}

ast::DefineModifierStmt Parser::parseDefineModifierStmt() {
    debug("Parsing define_modifier");
    consume(TokenType::DefineModifier);
    Token nameToken = tokenizer.next();
    if (nameToken.type != TokenType::Modifier && nameToken.type != TokenType::Key) {
        throw std::runtime_error("Expected custom modifier name after define_modifier");
    }
    std::string customName = nameToken.text;
    Token eqToken = consume(TokenType::Equals);
    ast::DefineModifierStmt stmt;
    stmt.name = customName;
    int currentRow = eqToken.row;
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
        throw std::runtime_error("No expansions found for custom modifier '" + customName + "'");
    }
    return stmt;
}

ast::ConfigPropertyStmt Parser::parseConfigPropertyStmt() {
    debug("Parsing a config property");
    Token cpToken = consume(TokenType::ConfigProperty);
    consume(TokenType::Equals);
    // we dont have an int token, so its either a key or a modifier
    Token intToken = consume(TokenType::Key);
    if (intToken.type != TokenType::Modifier && intToken.type != TokenType::Key) {
        throw std::runtime_error("Expected integer after '='");
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
    Token tk;
    while (true) {
        tk = tokenizer.next();
        if (tk.type == TokenType::Key || tk.type == TokenType::Literal) {
            ast::KeyAtom atom;
            if (tk.type == TokenType::Literal) {
                if (auto lit = tryParseLiteralKey(tk.text)) atom.value = *lit;
                else atom.value = ast::KeyChar{tk.text.empty() ? '\0' : tk.text[0], false};
            } else {
                atom.value = ast::KeyChar{tk.text[0], false};
            }
            ks.items.push_back(atom);
            tk = tokenizer.next();
            if (tk.type == TokenType::Comma) {
                continue;
            }
            if (tk.type == TokenType::CloseBrace) {
                break;
            }
            throw std::runtime_error("Unexpected token in brace expansion");
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
                throw std::runtime_error("Unexpected end of file");
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
                    else atom.value = ast::KeyChar{tk.text.empty() ? '\0' : tk.text[0], false};
                } else if (tk.type == TokenType::Key) {
                    atom.value = ast::KeyChar{tk.text[0], false};
                } else {
                    try {
                        unsigned long v = std::stoul(tk.text, nullptr, 16);
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
    Token nextTk = consume(TokenType::Command);
    stmt.syntax = std::move(syntax);
    stmt.command = nextTk.text;
    return stmt;
}
