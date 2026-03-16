module;

#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

export module ast;
import keysym;
import modifier;

export namespace ast {

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

export template <>
struct std::formatter<ast::ModifierAtom> : std::formatter<std::string_view> {
    auto format(const ast::ModifierAtom& m, std::format_context& ctx) const {
        return std::visit(
            [&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, BuiltinModifier>) {
                    return std::format_to(ctx.out(), "{}", v);
                } else {
                    // For custom modifier strings, output directly (matching original logic)
                    return std::format_to(ctx.out(), "{}", v);
                }
            },
            m.value);
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
        std::string result = "{";
        for (size_t i = 0; i < ks.items.size(); i++) {
            if (i > 0) result += ", ";
            result += std::format("{}", ks.items[i]);
        }
        result += "}";
        return std::format_to(ctx.out(), "{}", result);
    }
};

template <>
struct std::formatter<ast::ChordSyntax> : std::formatter<std::string_view> {
    auto format(const ast::ChordSyntax& cs, std::format_context& ctx) const {
        std::string result;
        // Format modifiers with " + " after each (matching original logic)
        for (const auto& mod : cs.modifiers) {
            result += std::format("{}", mod);
            result += " + ";
        }
        // Format key (or missing key)
        if (cs.key) {
            result += std::format("{}", *cs.key);
        } else {
            result += "<missing-key>";
        }
        return std::format_to(ctx.out(), "{}", result);
    }
};

template <>
struct std::formatter<ast::HotkeySyntax> : std::formatter<std::string_view> {
    auto format(const ast::HotkeySyntax& syn, std::format_context& ctx) const {
        std::string result;
        if (syn.passthrough) result += "passthrough, ";
        if (syn.repeat) result += "repeat, ";
        if (syn.onRelease) result += "onRelease, ";
        result += "[";
        for (size_t i = 0; i < syn.chords.size(); i++) {
            if (i > 0) result += "; ";
            result += std::format("{}", syn.chords[i]);
        }
        result += "]";
        return std::format_to(ctx.out(), "{}", result);
    }
};

template <>
struct std::formatter<ast::DefineModifierStmt> : std::formatter<std::string_view> {
    auto format(const ast::DefineModifierStmt& stmt, std::format_context& ctx) const {
        std::string result = "define_modifier: " + stmt.name + " = ";
        for (size_t i = 0; i < stmt.parts.size(); i++) {
            if (i > 0) result += " + ";
            result += std::format("{}", stmt.parts[i]);
        }
        return std::format_to(ctx.out(), "{}", result);
    }
};

template <>
struct std::formatter<ast::ConfigPropertyStmt> : std::formatter<std::string_view> {
    auto format(const ast::ConfigPropertyStmt& stmt, std::format_context& ctx) const {
        std::string renderedValue;
        if (stmt.intValue) {
            renderedValue = std::format("{}", *stmt.intValue);
        } else if (!stmt.listValues.empty()) {
            renderedValue = "[";
            for (size_t i = 0; i < stmt.listValues.size(); i++) {
                if (i > 0) renderedValue += ", ";
                renderedValue += std::format("\"{}\"", stmt.listValues[i]);
            }
            renderedValue += "]";
        } else {
            renderedValue = "<unset>";
        }
        return std::format_to(ctx.out(), "config: {} = {}", stmt.name, renderedValue);
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
        std::string result = "Program{\n";
        for (const auto& s : program.statements) {
            result += "  ";
            std::visit([&](const auto& node) { result += std::format("{}\n", node); }, s);
        }
        result += "}\n";
        return std::format_to(ctx.out(), "{}", result);
    }
};
