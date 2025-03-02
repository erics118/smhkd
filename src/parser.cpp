#include "parser.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hotkey.hpp"
#include "log.hpp"
#include "tokenizer.hpp"

std::vector<Hotkey> Parser::parseFile() {
    std::vector<Hotkey> hotkeys;

    // Keep parsing until EndOfFile
    while (m_tokenizer.hasMoreTokens()) {
        debug("Parsing next token");
        Token tk = m_tokenizer.peekToken();
        if (tk.type == TokenType::EndOfFile) {
            break;
        }
        // If line starts with "define_modifier", parse a custom modifier
        if (tk.type == TokenType::DefineModifier) {
            debug("Parsing define_modifier");
            parseDefineModifier();
        }
        // Otherwise, parse a hotkey
        else {
            Hotkey hk = parseHotkey();
            if (!hk.modifiers.empty() || !hk.key.empty() || !hk.command.empty()) {
                hotkeys.push_back(hk);
            }
        }
    }

    return hotkeys;
}

void Parser::parseDefineModifier() {
    debug("Parsing define_modifier");
    // consume the "define_modifier"
    Token dmToken = m_tokenizer.nextToken();  // define_modifier
    if (dmToken.type != TokenType::DefineModifier) {
        throw std::runtime_error("Expected 'define_modifier' token");
    }

    // Next token should be the name of the custom modifier
    Token nameToken = m_tokenizer.nextToken();
    if (nameToken.type != TokenType::Modifier && nameToken.type != TokenType::Key) {
        throw std::runtime_error("Expected custom modifier name after define_modifier");
    }
    std::string customName = nameToken.text;

    // Next token should be '='
    Token eqToken = m_tokenizer.nextToken();
    if (eqToken.type != TokenType::Equals) {
        throw std::runtime_error("Expected '=' after custom modifier name");
    }

    // Now read a sequence of existing modifiers, separated by plus.
    // e.g. "cmd + ctrl" or "shift + ham" or "cmd" alone, etc.
    std::vector<std::string> expansions;

    bool expectModifier = true;
    int currentRow = eqToken.row;  // Track the row we started on

    while (true) {
        // Peek next
        const Token& tk = m_tokenizer.peekToken();
        if (tk.type == TokenType::EndOfFile) {
            // no more tokens
            break;
        }

        // Stop if we've moved to a new line
        if (tk.row != currentRow) {
            break;
        }

        if (tk.type == TokenType::Plus) {
            // skip plus
            m_tokenizer.nextToken();
            expectModifier = true;
            continue;
        }

        // If it looks like a valid "modifier" or "key," consume it
        if (tk.type == TokenType::Modifier || tk.type == TokenType::Key) {
            expansions.push_back(tk.text);
            m_tokenizer.nextToken();
            expectModifier = false;
            continue;
        }
        // Otherwise, we break because we presumably reached a new line or new statement
        break;
    }

    if (expansions.empty()) {
        throw std::runtime_error("No expansions found for custom modifier '" + customName + "'");
    }

    // Store in the map
    debug("Storing custom modifier '{}' with expansions: {}", customName, expansions);
    m_customModifiers[customName] = expansions;
}

Hotkey Parser::parseHotkey() {
    Hotkey hk;
    bool foundColon = false;

    // Read tokens until we see a colon or EOF
    while (true) {
        const Token& tk = m_tokenizer.peekToken();
        if (tk.type == TokenType::EndOfFile) {
            return hk;
        }
        if (tk.type == TokenType::Colon) {
            // consume colon
            m_tokenizer.nextToken();
            foundColon = true;
            break;
        }
        if (tk.type == TokenType::Plus) {
            m_tokenizer.nextToken();
            continue;
        }
        if (tk.type == TokenType::EventType) {
            hk.eventType = parseEventType(tk.text);
            m_tokenizer.nextToken();
            continue;
        }
        if (tk.type == TokenType::Modifier) {
            // Could be built-in or custom
            auto expanded = expandModifier(tk.text, tk.row, tk.col);
            for (auto& e : expanded) {
                hk.modifiers.push_back(e);
            }
            m_tokenizer.nextToken();
            continue;
        }
        if (tk.type == TokenType::Key) {
            // single char => the 'key'
            hk.key = tk.text;
            m_tokenizer.nextToken();
            continue;
        }
        break;  // Something else => end or error
    }

    // If we found colon, next token might be Command
    if (foundColon) {
        const Token& nextTk = m_tokenizer.nextToken();
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

// expandModifier: if it's one of the built-ins or the "customModifiers" map
//    - If "cmd", "ctrl", "alt", or "shift", return that alone.
//    - Else if we have a user-defined name, expand it to the underlying list
//      (which may include built-in or *other* custom modifiers).
//    - Potentially do a recursive expansion if needed (be careful of infinite loops).
std::vector<std::string> Parser::expandModifier(const std::string& mod, int row, int col) {
    // If it's a built-in
    if (mod == "cmd" || mod == "ctrl" || mod == "alt" || mod == "shift") {
        return {mod};
    }

    // If custom
    auto it = m_customModifiers.find(mod);
    if (it != m_customModifiers.end()) {
        // Return expansions (we *could* also do a deeper expansion if needed)
        // For now, we do a shallow expansion, trusting that define_modifier
        // has only built-ins or previously-defined expansions.
        // If we wanted full recursion, we'd map them again, but that can lead
        // to cycles if not careful.
        std::vector<std::string> expandedAll;
        for (auto& sub : it->second) {
            // If sub is also a custom, we could recursively expand
            // but let's do a single-level expansion for demonstration:
            if (sub == "cmd" || sub == "ctrl" || sub == "alt" || sub == "shift") {
                expandedAll.push_back(sub);
            } else {
                error("AnUnknown modifier '{}' at row {}, col {}", sub, row, col);
            }
        }
        return expandedAll;
    }

    // If we get here, it's not built-in and not in custom map => error
    throw std::runtime_error(
        "Unknown modifier '" + mod + "' at row " + std::to_string(row) + ", col " + std::to_string(col));
}
