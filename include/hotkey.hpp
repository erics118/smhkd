#pragma once

#include <print>
#include <string>
#include <vector>

enum class KeyEventType {
    Down,  // default
    Up,
    Both,
};

// Specialize std::formatter for KeyEventType
template <>
struct std::formatter<KeyEventType> : std::formatter<std::string_view> {
    auto format(KeyEventType et, std::format_context& ctx) const {
        std::string_view name;
        switch (et) {
            case KeyEventType::Down: name = "Down"; break;
            case KeyEventType::Up: name = "Up"; break;
            case KeyEventType::Both: name = "Both"; break;
            default: name = "Unknown";
        }
        return std::format_to(ctx.out(), "{}", name);
    }
};

struct CustomModifier {
    std::string name;
    int flags;  // Combined flags of constituent modifiers
};

struct Hotkey {
    std::vector<std::string> modifiers;
    std::string key;
    std::string command;
    KeyEventType eventType = KeyEventType::Down;
};

template <>
struct std::formatter<Hotkey> : std::formatter<std::string_view> {
    auto format(Hotkey hk, std::format_context& ctx) const {
        // join modifiers with " + "
        std::string modifiers_str;
        for (const auto& mod : hk.modifiers) {
            modifiers_str += mod + " + ";
        }
        // print out each part
        std::format_to(ctx.out(), "{}{} ({}): {}", modifiers_str, hk.key, hk.eventType, hk.command);
        return ctx.out();
    }
};
