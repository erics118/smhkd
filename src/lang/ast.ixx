module;

#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

export module smhkd.ast;
import smhkd.keysym;
import smhkd.modifier;

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
    int value{};
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
