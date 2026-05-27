#pragma once

#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "../input/keysym.hpp"
#include "../input/modifier.hpp"

namespace ast {

struct ModifierAtom {
    std::variant<BuiltinModifier, std::string> value;
};

struct DefineModifierStmt {
    std::string name;
    std::vector<ModifierAtom> parts;
};

struct ConfigPropertyStmt {
    std::string name;
    std::optional<int> intValue;
    std::vector<std::string> listValues;
};

struct KeyChar {
    char value;
    bool isHex;
};

struct KeyAtom {
    std::variant<LiteralKey, KeyChar> value;
};

struct KeySyntax {
    bool isBraceExpansion{false};
    std::vector<KeyAtom> items;
};

struct ChordSyntax {
    std::vector<ModifierAtom> modifiers;
    std::optional<KeySyntax> key;
};

struct HotkeySyntax {
    bool passthrough{};
    bool repeat{};
    bool onRelease{};
    std::vector<ChordSyntax> chords;
};

struct HotkeyStmt {
    HotkeySyntax syntax;
    std::string command;
};

using Stmt = std::variant<DefineModifierStmt, ConfigPropertyStmt, HotkeyStmt>;

struct Program {
    std::vector<Stmt> statements;
};

}  // namespace ast

template <>
struct std::formatter<ast::ModifierAtom> : std::formatter<std::string_view> {
    auto format(const ast::ModifierAtom& m, std::format_context& ctx) const {
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
struct ::std::formatter<ast::KeyAtom> : std::formatter<std::string_view> {
    auto format(const ast::KeyAtom& atom, std::format_context& ctx) const {
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
            atom.value);
    }
};

template <>
struct std::formatter<ast::KeySyntax> : std::formatter<std::string_view> {
    auto format(const ast::KeySyntax& ks, std::format_context& ctx) const {
        if (ks.items.empty()) {
            return std::format_to(ctx.out(), "<none>");
        }
        if (!ks.isBraceExpansion) {
            return std::format_to(ctx.out(), "{}", ks.items.front());
        }
        auto out = std::format_to(ctx.out(), "{{");
        for (size_t i = 0; i < ks.items.size(); i++) {
            if (i > 0) out = std::format_to(out, ", ");
            out = std::format_to(out, "{}", ks.items[i]);
        }
        return std::format_to(out, "}}");
    }
};

template <>
struct std::formatter<ast::ChordSyntax> : std::formatter<std::string_view> {
    auto format(const ast::ChordSyntax& cs, std::format_context& ctx) const {
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
struct std::formatter<ast::HotkeySyntax> : std::formatter<std::string_view> {
    auto format(const ast::HotkeySyntax& syn, std::format_context& ctx) const {
        auto out = ctx.out();
        if (syn.passthrough) out = std::format_to(out, "passthrough, ");
        if (syn.repeat) out = std::format_to(out, "repeat, ");
        if (syn.onRelease) out = std::format_to(out, "onRelease, ");
        out = std::format_to(out, "[");
        for (size_t i = 0; i < syn.chords.size(); i++) {
            if (i > 0) out = std::format_to(out, "; ");
            out = std::format_to(out, "{}", syn.chords[i]);
        }
        return std::format_to(out, "]");
    }
};

template <>
struct std::formatter<ast::DefineModifierStmt> : std::formatter<std::string_view> {
    auto format(const ast::DefineModifierStmt& stmt, std::format_context& ctx) const {
        auto out = std::format_to(ctx.out(), "define_modifier: {} = ", stmt.name);
        for (size_t i = 0; i < stmt.parts.size(); i++) {
            if (i > 0) out = std::format_to(out, " + ");
            out = std::format_to(out, "{}", stmt.parts[i]);
        }
        return out;
    }
};

template <>
struct std::formatter<ast::ConfigPropertyStmt> : std::formatter<std::string_view> {
    auto format(const ast::ConfigPropertyStmt& stmt, std::format_context& ctx) const {
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
struct std::formatter<ast::HotkeyStmt> : std::formatter<std::string_view> {
    auto format(const ast::HotkeyStmt& stmt, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "hotkey: {} \"{}\"", stmt.syntax, stmt.command);
    }
};

template <>
struct std::formatter<ast::Program> : std::formatter<std::string_view> {
    auto format(const ast::Program& program, std::format_context& ctx) const {
        auto out = std::format_to(ctx.out(), "Program{{\n");
        for (const auto& s : program.statements) {
            out = std::format_to(out, "  ");
            out = std::visit([&](const auto& node) { return std::format_to(out, "{}\n", node); }, s);
        }
        return std::format_to(out, "}}\n");
    }
};
