#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "keysym.hpp"
#include "modifier.hpp"

// A minimal AST for the smhkd configuration DSL. The goal is to
// separate syntax from execution so the interpreter can evaluate it.

// define_modifier NAME = modifiers+
struct ModifierAtom {
    std::variant<BuiltinModifier, std::string> value;  // builtin or custom name
};

struct DefineModifierStmt {
    std::string name;
    std::vector<ModifierAtom> parts;  // modifier references (builtin or custom)
};

// name = integer
struct ConfigPropertyStmt {
    std::string name;
    int value{};  // milliseconds for current properties
};

// Distinguish between normal key chars and hex-derived chars
struct KeyChar {
    char value;
    bool isHex;  // true if originated from a hex token
};

// Key atoms use enums/typed chars
struct KeyAtom {
    std::variant<LiteralKey, KeyChar> value;  // literal or key char (possibly from hex)
};

// A keysym in syntax: either a single key or a brace expansion list
struct KeySyntax {
    bool isBraceExpansion{false};
    std::vector<KeyAtom> items;  // single item when not a brace expansion
};

// One chord: textual modifiers and a keysym (possibly unset while parsing a sequence)
struct ChordSyntax {
    std::vector<ModifierAtom> modifiers;
    std::optional<KeySyntax> key;  // required for non-sequence partials; present when encountered before ':'
};

// Hotkey syntax with flags and chord list
struct HotkeySyntax {
    bool passthrough{};
    bool repeat{};
    bool onRelease{};
    std::vector<ChordSyntax> chords;  // one or more
};

// full hotkey statement: chords : command
struct HotkeyStmt {
    HotkeySyntax syntax;
    std::string command;  // raw command (may include brace content)
};

using Stmt = std::variant<DefineModifierStmt, ConfigPropertyStmt, HotkeyStmt>;

struct Program {
    std::vector<Stmt> statements;
};
