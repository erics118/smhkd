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

std::unordered_map<Hotkey, std::string> Parser::parseFile() {
    std::unordered_map<Hotkey, std::string> hotkeys;

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

int Parser::getImplicitFlags(const std::string& literal) {
    const auto* it = std::ranges::find(literal_keycode_str, literal);
    auto literal_index = std::distance(literal_keycode_str.begin(), it);

    int flags{};
    if ((literal_index > key_has_implicit_fn_mod) && (literal_index < key_has_implicit_nx_mod)) {
        flags = Hotkey_Flag_Fn;
    } else if (literal_index >= key_has_implicit_nx_mod) {
        flags = Hotkey_Flag_NX;
    }
    return flags;
}

KeyEventType Parser::parseEventType(const std::string& text) {
    if (text == "up") return KeyEventType::Up;
    if (text == "down") return KeyEventType::Down;
    if (text == "both") return KeyEventType::Both;
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

    error("Unknown modifier '{}' at row {}, col {}", mod, row, col);
    return 0;
}

bool Parser::isModifier(const std::string& mod) {
    return getBuiltinModifierFlag(mod) != 0 || getCustomModifierFlag(mod, 0, 0) != 0;
}

std::vector<std::string> Parser::parseKeyBraceExpansion() {
    std::vector<std::string> items;
    std::string current;

    // Consume opening brace
    Token tk = tokenizer.next();
    if (tk.type != TokenType::OpenBrace) {
        throw std::runtime_error("Expected opening brace");
    }

    while (true) {
        tk = tokenizer.next();

        if (tk.type == TokenType::CloseBrace) {
            if (!current.empty()) {
                items.push_back(current);
            }
            break;
        }

        if (tk.type == TokenType::Comma) {
            if (!current.empty()) {
                items.push_back(current);
                current.clear();
            }
            continue;
        }

        if (tk.type == TokenType::Key || tk.type == TokenType::Identifier || tk.type == TokenType::Literal) {
            current += tk.text;
        } else {
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

    debug("Expanded command string '{}' to {}", command, result);

    return result;
}

std::vector<std::pair<Hotkey, std::string>> Parser::expandHotkey(const Hotkey& base, const std::vector<std::string>& items, const std::vector<std::string>& expandedCommands) {
    std::vector<std::pair<Hotkey, std::string>> expanded;

    for (int i = 0; i < items.size(); i++) {
        const auto& item = items[i];
        Hotkey hk = base;
        std::string command = expandedCommands.empty() ? "" : expandedCommands[i];

        if (std::ranges::contains(literal_keycode_str, item)) {
            hk.keyCode = getKeycode(item);
            hk.flags |= getImplicitFlags(item);
        } else if (item.size() == 1) {
            hk.keyCode = getKeycode(item);
        } else {
            // Try to parse as hex if it's not a literal or single char
            try {
                hk.keyCode = std::stoi(item, nullptr, 16);
            } catch (...) {
                throw std::runtime_error("Invalid key in expansion: " + item);
            }
        }
        expanded.push_back({hk, command});
    }

    return expanded;
}

std::vector<std::pair<Hotkey, std::string>> Parser::parseHotkeyWithExpansion() {
    std::vector<Hotkey> sequence;
    Hotkey base;
    std::string command;
    std::vector<std::string> expansionItems;
    std::vector<std::string> expandedCommands;
    bool foundColon = false;

    while (true) {
        // Parse a single hotkey in the sequence
        while (true) {
            const Token& tk = tokenizer.peek();
            if (tk.type == TokenType::EndOfFile) {
                error("Unexpected end of file");
                if (sequence.empty()) {
                    return {{base, command}};
                }
                base.sequence = sequence;
                return {{base, command}};
            }
            if (tk.type == TokenType::Colon) {
                tokenizer.next();
                foundColon = true;
                break;
            }
            if (tk.type == TokenType::Semicolon) {
                tokenizer.next();
                sequence.push_back(base);
                base = Hotkey{};  // Reset for next in sequence
                break;
            }
            if (tk.type == TokenType::Plus) {
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::At) {
                base.passthrough = true;
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::Repeat) {
                base.repeat = true;
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::EventType) {
                base.eventType = parseEventType(tk.text);
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::Modifier) {
                base.flags |= getModifierFlag(tk.text, tk.row, tk.col);
                tokenizer.next();
                continue;
            }
            if (tk.type == TokenType::OpenBrace) {
                expansionItems = parseKeyBraceExpansion();
                continue;
            }
            if (tk.type == TokenType::Literal || tk.type == TokenType::Key || tk.type == TokenType::KeyHex) {
                if (tk.type == TokenType::Literal) {
                    base.keyCode = getKeycode(tk.text);
                    base.flags |= getImplicitFlags(tk.text);
                } else if (tk.type == TokenType::Key) {
                    base.keyCode = getKeycode(tk.text);
                } else {
                    base.keyCode = std::stoi(tk.text, nullptr, 16);
                }
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
            if (expansionItems.size() > 1) {
                // expand the command
                expandedCommands = expandCommandString(nextTk.text);
            } else {
                command = nextTk.text;
            }
        } else {
            throw std::runtime_error("Expected command after colon");
        }
    }

    // If we found expansion items, expand the base hotkey
    if (!expansionItems.empty()) {
        auto expanded = expandHotkey(base, expansionItems, expandedCommands);
        // Apply sequence to all expanded hotkeys
        if (!sequence.empty()) {
            for (auto& [hk, command] : expanded) {
                hk.sequence = sequence;
            }
        }
        return expanded;
    }

    // If we have a sequence, add it to base
    if (!sequence.empty()) {
        sequence.push_back(base);
        base.sequence = sequence;
    }

    return {{base, command}};
}
