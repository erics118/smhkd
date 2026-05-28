#pragma once

#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "../input/keysym.hpp"
#include "../input/modifier.hpp"

namespace ast {

struct Modifier {
    std::variant<BuiltinModifier, std::string> value;
};

struct DefineModifier {
    std::string name;
    std::vector<Modifier> parts;
};

struct ConfigProperty {
    std::string name;
    std::optional<int> intValue;
    std::vector<std::string> listValues;
};

struct KeyChar {
    char value;
    bool isHex;
};

struct SimpleKeysym {
    std::variant<LiteralKey, KeyChar> value;
};

struct BraceExpansionKeysym {
    std::vector<SimpleKeysym> alternatives;
};

using Keysym = std::variant<SimpleKeysym, BraceExpansionKeysym>;

inline bool isBrace(const Keysym& k) {
    return std::holds_alternative<BraceExpansionKeysym>(k);
}
inline const BraceExpansionKeysym* asBrace(const Keysym& k) {
    return std::get_if<BraceExpansionKeysym>(&k);
}
inline const SimpleKeysym* asSimple(const Keysym& k) {
    return std::get_if<SimpleKeysym>(&k);
}

struct Chord {
    std::vector<Modifier> modifiers;
    std::optional<Keysym> key;
};

struct Chords {
    bool passthrough{};
    bool repeat{};
    bool onRelease{};
    std::vector<Chord> sequence;
};

struct Hotkey {
    Chords chords;
    std::string command;
};

struct Remap {
    Chords source;
    Chord target;
};

using Stmt = std::variant<DefineModifier, ConfigProperty, Hotkey, Remap>;

struct Program {
    std::vector<Stmt> statements;
};

}  // namespace ast

template <>
struct std::formatter<ast::Modifier> : std::formatter<std::string_view> {
    auto format(const ast::Modifier& m, std::format_context& ctx) const {
        return std::visit([&](const auto& v) { return std::format_to(ctx.out(), "{}", v); }, m.value);
    }
};

template <>
struct ::std::formatter<ast::KeyChar> : std::formatter<std::string_view> {
    auto format(const ast::KeyChar& kc, std::format_context& ctx) const {
        if (kc.isHex) {
            return std::format_to(ctx.out(), "key:{:02x}", static_cast<unsigned char>(kc.value));
        }
        return std::format_to(ctx.out(), "key:{}", kc.value);
    }
};

template <>
struct ::std::formatter<ast::SimpleKeysym> : std::formatter<std::string_view> {
    auto format(const ast::SimpleKeysym& ks, std::format_context& ctx) const {
        return std::visit(
            [&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, LiteralKey>) {
                    return std::format_to(ctx.out(), "literal: {}", v);
                } else if constexpr (std::is_same_v<T, ast::KeyChar>) {
                    return std::format_to(ctx.out(), "{}", v);
                }
                return ctx.out();
            },
            ks.value);
    }
};

template <>
struct std::formatter<ast::Keysym> : std::formatter<std::string_view> {
    auto format(const ast::Keysym& ks, std::format_context& ctx) const {
        if (std::holds_alternative<ast::SimpleKeysym>(ks)) {
            return std::format_to(ctx.out(), "{}", std::get<ast::SimpleKeysym>(ks));
        }
        if (std::holds_alternative<ast::BraceExpansionKeysym>(ks)) {
            const auto& be = std::get<ast::BraceExpansionKeysym>(ks);
            if (be.alternatives.empty()) {
                return std::format_to(ctx.out(), "<empty-brace-expansion>");
            }
            auto out = std::format_to(ctx.out(), "{{");
            for (size_t i = 0; i < be.alternatives.size(); i++) {
                if (i > 0) out = std::format_to(out, ", ");
                out = std::format_to(out, "{}", be.alternatives[i]);
            }
            return std::format_to(out, "}}");
        }
        return ctx.out();
    }
};

template <>
struct std::formatter<ast::Chord> : std::formatter<std::string_view> {
    auto format(const ast::Chord& cs, std::format_context& ctx) const {
        auto out = ctx.out();
        for (const auto& mod : cs.modifiers) {
            out = std::format_to(out, "{} + ", mod);
        }
        if (cs.key) {
            return std::format_to(out, "{}", *cs.key);
        }
        return std::format_to(out, "<missing-key>");
    }
};

template <>
struct std::formatter<ast::Chords> : std::formatter<std::string_view> {
    auto format(const ast::Chords& syn, std::format_context& ctx) const {
        auto out = ctx.out();
        if (syn.passthrough) out = std::format_to(out, "passthrough, ");
        if (syn.repeat) out = std::format_to(out, "repeat, ");
        if (syn.onRelease) out = std::format_to(out, "onRelease, ");
        out = std::format_to(out, "[");
        for (size_t i = 0; i < syn.sequence.size(); i++) {
            if (i > 0) out = std::format_to(out, "; ");
            out = std::format_to(out, "{}", syn.sequence[i]);
        }
        return std::format_to(out, "]");
    }
};

template <>
struct std::formatter<ast::DefineModifier> : std::formatter<std::string_view> {
    auto format(const ast::DefineModifier& stmt, std::format_context& ctx) const {
        auto out = std::format_to(ctx.out(), "define_modifier: {} = ", stmt.name);
        for (size_t i = 0; i < stmt.parts.size(); i++) {
            if (i > 0) out = std::format_to(out, " + ");
            out = std::format_to(out, "{}", stmt.parts[i]);
        }
        return out;
    }
};

template <>
struct std::formatter<ast::ConfigProperty> : std::formatter<std::string_view> {
    auto format(const ast::ConfigProperty& stmt, std::format_context& ctx) const {
        auto out = std::format_to(ctx.out(), "config: {} = ", stmt.name);
        if (stmt.intValue) {
            return std::format_to(out, "{}", *stmt.intValue);
        }
        if (!stmt.listValues.empty()) {
            out = std::format_to(out, "[");
            for (size_t i = 0; i < stmt.listValues.size(); i++) {
                if (i > 0) out = std::format_to(out, ", ");
                out = std::format_to(out, "\"{}\"", stmt.listValues[i]);
            }
            return std::format_to(out, "]");
        }
        return std::format_to(out, "<unset>");
    }
};

template <>
struct std::formatter<ast::Hotkey> : std::formatter<std::string_view> {
    auto format(const ast::Hotkey& stmt, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "hotkey: {} \"{}\"", stmt.chords, stmt.command);
    }
};

template <>
struct std::formatter<ast::Remap> : std::formatter<std::string_view> {
    auto format(const ast::Remap& stmt, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "remap: {} | {}", stmt.source, stmt.target);
    }
};

template <>
struct std::formatter<ast::Program> : std::formatter<std::string_view> {
    auto format(const ast::Program& program, std::format_context& ctx) const {
        auto out = std::format_to(ctx.out(), "Program {{\n");
        for (const auto& s : program.statements) {
            out = std::format_to(out, "  ");
            out = std::visit([&](const auto& node) { return std::format_to(out, "{}\n", node); }, s);
        }
        return std::format_to(out, "}}\n");
    }
};
