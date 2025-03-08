#include "parser.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "hotkey.hpp"
#include "log.hpp"
#include "tokenizer.hpp"

std::map<Hotkey, std::string> Parser::parseFile() {
    std::map<Hotkey, std::string> hotkeys;

    while (tokenizer.hasMoreTokens()) {
        Token tk = tokenizer.peek();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }
        if (tk.type == TokenType::DefineModifier) {
            parseDefineModifier();
        } else {
            auto hks = parseHotkeyWithExpansion();
            for (const auto& [hk, command] : hks) {
                debug("Storing hotkey: {} : {}", hk, command);
                hotkeys[hk] = command;
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

int Parser::getBuiltinModifierFlag(const std::string& mod) {
    const auto* it1 = std::ranges::find(hotkey_flag_names, mod);
    if (it1 != hotkey_flag_names.end()) {
        return hotkey_flags[it1 - hotkey_flag_names.begin()];
    }
    return 0;
}

// NOLINTNEXTLINE(misc-no-recursion)
int Parser::getCustomModifierFlag(const std::string& mod, int row, int col) {
    int flags{};
    auto it = customModifiers.find(mod);
    if (it != customModifiers.end()) {
        for (auto& sub : it->second) {
            int subFlags = getModifierFlag(sub, row, col);

            if (subFlags == 0) return 0;

            flags |= subFlags;
        }
        return flags;
    }
    return 0;
}

// NOLINTNEXTLINE(misc-no-recursion)
int Parser::getModifierFlag(const std::string& mod, int row, int col) {
    int flags{};

    flags = getBuiltinModifierFlag(mod);
    if (flags != 0) return flags;

    flags = getCustomModifierFlag(mod, row, col);
    if (flags != 0) return flags;

    warn("Unknown modifier '{}' at row {}, col {}", mod, row, col);
    return 0;
}

bool Parser::isModifier(const std::string& mod) {
    return getBuiltinModifierFlag(mod) != 0 || getCustomModifierFlag(mod, 0, 0) != 0;
}

std::vector<Token> Parser::parseKeyBraceExpansion() {
    std::vector<Token> items;
    Token current;

    // Consume opening brace
    Token tk = tokenizer.next();
    if (tk.type != TokenType::OpenBrace) {
        throw std::runtime_error("Expected opening brace");
    }

    while (true) {
        tk = tokenizer.next();

        if (tk.type == TokenType::Key || tk.type == TokenType::Literal) {
            current = tk;

            tk = tokenizer.next();
            if (tk.type == TokenType::Comma) {
                items.push_back(current);
                continue;
            }
            if (tk.type == TokenType::CloseBrace) {
                items.push_back(current);
                break;
            }
            throw std::runtime_error("Unexpected token in brace expansion");
        }
    }

    return items;
}

std::vector<std::string> Parser::expandCommandString(const std::string& command) {
    std::vector<std::string> result;

    // Find the positions of '{' and '}'
    size_t start = command.find('{');
    size_t end = command.find('}');

    if (start == std::string::npos || end == std::string::npos || start > end) {
        // If no valid braces found, return the input as is
        result.push_back(command);
        return result;
    }

    // Extract prefix, content inside braces, and suffix
    std::string prefix = command.substr(0, start);
    std::string content = command.substr(start + 1, end - start - 1);
    std::string suffix = command.substr(end + 1);

    // Manually split content by commas
    size_t pos = 0;
    while (pos < content.size()) {
        size_t comma = content.find(',', pos);
        if (comma == std::string::npos) {
            comma = content.size();  // Last token
        }

        std::string token = content.substr(pos, comma - pos);
        result.push_back(prefix + token + suffix);

        pos = comma + 1;  // Move past the comma
    }

    // debug("Expanded command string '{}' to {}", command, result);

    return result;
}

std::vector<std::pair<Hotkey, std::string>> Parser::parseHotkeyWithExpansion() {
    // std::vector<Hotkey> hotkeys;
    Hotkey hotkey;
    std::string command;
    std::vector<Token> expansionKeysyms;
    std::vector<std::string> expandedCommands;
    bool foundColon = false;

    while (true) {
        // Parse a single hotkey in the chords
        while (true) {
            const Token& tk = tokenizer.peek();
            if (tk.type == TokenType::EndOfFile) {
                throw std::runtime_error("Unexpected end of file");
            }
            if (tk.type == TokenType::Colon) {
                tokenizer.next();
                foundColon = true;
                break;
            }
            if (tk.type == TokenType::Semicolon) {
                tokenizer.next();
                hotkey.chords.push_back(Chord{});  // Reset for next in sequence
                break;
            }
            if (tk.type == TokenType::Plus) {
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::At) {
                hotkey.passthrough = true;
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::Ampersand) {
                hotkey.repeat = true;
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::Caret) {
                hotkey.on_release = true;
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::Modifier) {
                hotkey.chords.back().modifiers.flags |= getModifierFlag(tk.text, tk.row, tk.col);
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::OpenBrace) {
                expansionKeysyms = parseKeyBraceExpansion();
                continue;
            }
            if (tk.type == TokenType::Literal || tk.type == TokenType::Key || tk.type == TokenType::KeyHex) {
                hotkey.chords.back().setKeycode(tk);
                tokenizer.next();
                continue;
            }
            break;
        }

        if (foundColon) {
            break;
        }
    }

    // if colon, next token should be command
    if (foundColon) {
        const Token& nextTk = tokenizer.next();
        if (nextTk.type == TokenType::Command) {
            if (expansionKeysyms.size() > 1) {
                // expand the command
                expandedCommands = expandCommandString(nextTk.text);
            } else {
                command = nextTk.text;
            }
        } else {
            throw std::runtime_error("Expected command after colon");
        }
    }

    // expand the keysyms
    if (!expansionKeysyms.empty()) {
        std::vector<std::pair<Hotkey, std::string>> expanded;
        if (expansionKeysyms.size() != expandedCommands.size()) {
            throw std::runtime_error("Expansion keysyms and commands must be the same size");
        }
        for (int i = 0; i < expansionKeysyms.size(); i++) {
            const auto& keysym = expansionKeysyms[i];
            const auto& command = expandedCommands.empty() ? "" : expandedCommands[i];
            Hotkey hk = hotkey;
            hk.chords.back().setKeycode(keysym);
            expanded.emplace_back(hk, command);
        }
        return expanded;
    }

    return {{hotkey, command}};
}
