module;

#include <optional>
#include <string>
#include <variant>
#include <vector>

export module smhkd.ast;
import smhkd.keysym;
import smhkd.modifier;

export struct ModifierAtom {
    std::variant<BuiltinModifier, std::string> value;
};

export struct DefineModifierStmt {
    std::string name;
    std::vector<ModifierAtom> parts;
};

export struct ConfigPropertyStmt {
    std::string name;
    int value{};
};

export struct KeyChar {
    char value;
    bool isHex;
};

export struct KeyAtom {
    std::variant<LiteralKey, KeyChar> value;
};

export struct KeySyntax {
    bool isBraceExpansion{false};
    std::vector<KeyAtom> items;
};

export struct ChordSyntax {
    std::vector<ModifierAtom> modifiers;
    std::optional<KeySyntax> key;
};

export struct HotkeySyntax {
    bool passthrough{};
    bool repeat{};
    bool onRelease{};
    std::vector<ChordSyntax> chords;
};

export struct HotkeyStmt {
    HotkeySyntax syntax;
    std::string command;
};

export using Stmt = std::variant<DefineModifierStmt, ConfigPropertyStmt, HotkeyStmt>;

export struct Program {
    std::vector<Stmt> statements;
};

