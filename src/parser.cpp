#include "parser.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hotkey.hpp"
#include "locale.hpp"
#include "log.hpp"
#include "tokenizer.hpp"

std::vector<Hotkey> Parser::parseFile() {
    std::vector<Hotkey> hotkeys;

    while (tokenizer.hasMoreTokens()) {
        Token tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }
        if (tk.type == TokenType::DefineModifier) {
            parseDefineModifier();
        } else {
            Hotkey hk = parseHotkey();
            if (!hk.command.empty()) {
                hotkeys.push_back(hk);
            }
        }
    }

    return hotkeys;
}

void Parser::parseDefineModifier() {
    debug("Parsing define_modifier");

    // consume the "define_modifier"
    Token dmToken = tokenizer.next();
    if (dmToken.type != TokenType::DefineModifier) {
        throw std::runtime_error("Expected 'define_modifier' token");
    }

    // next token should be the name of custom modifier
    Token nameToken = tokenizer.next();
    if (nameToken.type != TokenType::Modifier && nameToken.type != TokenType::Key) {
        throw std::runtime_error("Expected custom modifier name after define_modifier");
    }
    std::string customName = nameToken.text;

    // then equal sign
    Token eqToken = tokenizer.next();
    if (eqToken.type != TokenType::Equals) {
        throw std::runtime_error("Expected '=' after custom modifier name");
    }

    // then modifiers
    std::vector<std::string> expansions;

    bool expectModifier = true;
    int currentRow = eqToken.row;  // Track the row we started on

    while (true) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }

        // stop if new line
        if (tk.row != currentRow) {
            break;
        }

        if (tk.type == TokenType::Plus) {
            tokenizer.next();
            expectModifier = true;
            continue;
        }

        if (tk.type == TokenType::Modifier || tk.type == TokenType::Key) {
            expansions.push_back(tk.text);
            tokenizer.next();
            expectModifier = false;
            continue;
        }

        break;
    }

    if (expansions.empty()) {
        throw std::runtime_error("No expansions found for custom modifier '" + customName + "'");
    }

    // Store in the map
    debug("Storing custom modifier '{}' with expansions: {}", customName, expansions);
    customModifiers[customName] = expansions;
}

Hotkey Parser::parseHotkey() {
    debug("Parsing hotkey");
    Hotkey hk;
    bool foundColon = false;

    // read until eof or colon
    while (true) {
        const Token& tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            return hk;
        }
        if (tk.type == TokenType::Colon) {
            // consume colon
            tokenizer.next();
            foundColon = true;
            break;
        }
        if (tk.type == TokenType::Plus) {
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::At) {
            hk.passthrough = true;
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::Repeat) {
            hk.repeat = true;
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::EventType) {
            hk.eventType = parseEventType(tk.text);
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::Modifier) {
            hk.flags = getModifierFlag(tk.text, tk.row, tk.col);
            tokenizer.next();
            continue;
        }
        if (tk.type == TokenType::Key) {
            // single char => the 'key'
            hk.keyCode = getKeycode(tk.text);
            if (hk.keyCode == -1) {
                throw std::runtime_error("Invalid key: " + tk.text);
            }
            tokenizer.next();
            continue;
        }
        break;
    }

    // if colon, next token should be command
    if (foundColon) {
        const Token& nextTk = tokenizer.next();
        if (nextTk.type == TokenType::Command) {
            hk.command = nextTk.text;
        } else {
            throw std::runtime_error("Expected command after colon");
        }
    }

    return hk;
}

KeyEventType Parser::parseEventType(const std::string& text) {
    if (text == "~up") return KeyEventType::Up;
    if (text == "~down") return KeyEventType::Down;
    if (text == "~both") return KeyEventType::Both;
    return KeyEventType::Down;
}

int Parser::getBuiltinModifierFlag(const std::string& mod) {
    const auto* it1 = std::ranges::find(hotkey_flag_names, mod);
    if (it1 != hotkey_flag_names.end()) {
        return hotkey_flags[it1 - hotkey_flag_names.begin()];
    }
    return 0;
}

// NOLINTNEXTLINE(misc-no-recursion)
int Parser::getCustomModifierFlag(const std::string& mod, int row, int col) {
    int flags = 0;
    auto it = customModifiers.find(mod);
    if (it != customModifiers.end()) {
        // TODO: support simple recursive expansion
        for (auto& sub : it->second) {
            int subFlags = getModifierFlag(sub, row, col);
            if (subFlags == 0) {
                error("Unknown modifier '{}' at row {}, col {}", sub, row, col);
            }
            flags |= subFlags;
        }
        return flags;
    }
    error("Unknown modifier '{}' at row {}, col {}", mod, row, col);
    return 0;
}

// NOLINTNEXTLINE(misc-no-recursion)
int Parser::getModifierFlag(const std::string& mod, int row, int col) {
    int flags = 0;

    flags = getBuiltinModifierFlag(mod);
    if (flags != 0) return flags;

    flags = getCustomModifierFlag(mod, row, col);
    if (flags != 0) return flags;

    error("Unknown modifier '{}' at row {}, col {}", mod, row, col);
}
