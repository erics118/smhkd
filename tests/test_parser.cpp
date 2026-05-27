#include <string_view>

#include "doctest.h"
#include "input/modifier.hpp"
#include "lang/parser.hpp"

namespace {

const ast::KeyChar& key_char(const ast::KeySyntax& keySyntax, size_t index = 0) {
    return std::get<ast::KeyChar>(keySyntax.items.at(index).value);
}

BuiltinModifier builtin_modifier(const ast::ModifierAtom& atom) {
    return std::get<BuiltinModifier>(atom.value);
}

}  // namespace

TEST_CASE("simple hotkey parses full AST content") {
    Parser p{"cmd + a : echo hi"};
    auto program = p.parseProgram();

    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::HotkeyStmt>(program.statements[0]);
    CHECK(stmt.command == "echo hi");
    CHECK_FALSE(stmt.syntax.passthrough);
    CHECK_FALSE(stmt.syntax.repeat);
    CHECK_FALSE(stmt.syntax.onRelease);
    REQUIRE(stmt.syntax.chords.size() == 1);
    REQUIRE(stmt.syntax.chords[0].modifiers.size() == 1);
    CHECK(builtin_modifier(stmt.syntax.chords[0].modifiers[0]) == BuiltinModifier::Cmd);
    REQUIRE(stmt.syntax.chords[0].key.has_value());
    CHECK_FALSE(stmt.syntax.chords[0].key->isBraceExpansion);
    CHECK(key_char(*stmt.syntax.chords[0].key).value == 'a');
    CHECK_FALSE(key_char(*stmt.syntax.chords[0].key).isHex);
}

TEST_CASE("hotkey flags and multi-chord sequence preserve parser structure") {
    Parser p{"cmd + a ; shift + b ^ & @ : echo hi"};
    auto program = p.parseProgram();

    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::HotkeyStmt>(program.statements[0]);
    CHECK(stmt.command == "echo hi");
    CHECK(stmt.syntax.passthrough);
    CHECK(stmt.syntax.repeat);
    CHECK(stmt.syntax.onRelease);
    REQUIRE(stmt.syntax.chords.size() == 2);
    REQUIRE(stmt.syntax.chords[0].modifiers.size() == 1);
    REQUIRE(stmt.syntax.chords[1].modifiers.size() == 1);
    CHECK(builtin_modifier(stmt.syntax.chords[0].modifiers[0]) == BuiltinModifier::Cmd);
    CHECK(builtin_modifier(stmt.syntax.chords[1].modifiers[0]) == BuiltinModifier::Shift);
    REQUIRE(stmt.syntax.chords[0].key.has_value());
    REQUIRE(stmt.syntax.chords[1].key.has_value());
    CHECK(key_char(*stmt.syntax.chords[0].key).value == 'a');
    CHECK(key_char(*stmt.syntax.chords[1].key).value == 'b');
}

TEST_CASE("unknown config assignment at line start parses as ConfigPropertyStmt") {
    Parser p{"unknown_setting = 7"};
    auto program = p.parseProgram();

    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::ConfigPropertyStmt>(program.statements[0]);
    CHECK(stmt.name == "unknown_setting");
    REQUIRE(stmt.intValue.has_value());
    CHECK(*stmt.intValue == 7);
}

TEST_CASE("multi-digit integer in key position emits error and recovers") {
    Parser p{"cmd + 10 : echo bad\ncmd + 1 : echo good"};
    auto program = p.parseProgram();

    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("'10' is not a valid key"));
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::HotkeyStmt>(program.statements[0]);
    CHECK(stmt.command == "echo good");
    REQUIRE(stmt.syntax.chords.size() == 1);
    REQUIRE(stmt.syntax.chords[0].key.has_value());
    CHECK(key_char(*stmt.syntax.chords[0].key).value == '1');
}

TEST_CASE("scalar config requires Integer token") {
    Parser p{"max_chord_interval = 500"};
    auto program = p.parseProgram();

    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::ConfigPropertyStmt>(program.statements[0]);
    CHECK(stmt.name == "max_chord_interval");
    REQUIRE(stmt.intValue.has_value());
    CHECK(*stmt.intValue == 500);
    CHECK(stmt.listValues.empty());
}

TEST_CASE("scalar config rejects non-integer value") {
    Parser p{"max_chord_interval = abc"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(!p.errors().empty());
    CHECK(p.errors()[0].message.contains("Expected integer"));
}

TEST_CASE("scalar config rejects out-of-range integer value") {
    Parser p{"max_chord_interval = 999999999999999999999"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("out of range"));
}

TEST_CASE("scalar config reports trailing tokens after value") {
    Parser p{"max_chord_interval = 500 nope"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::ConfigPropertyStmt>(program.statements[0]);
    REQUIRE(stmt.intValue.has_value());
    CHECK(*stmt.intValue == 500);
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("after config property 'max_chord_interval'"));
    CHECK(p.errors()[0].message.contains("nope"));
}

TEST_CASE("blacklist parses string and identifier entries") {
    Parser p{R"(blacklist = ["Terminal", iTerm2, 42])"};
    auto program = p.parseProgram();

    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::ConfigPropertyStmt>(program.statements[0]);
    REQUIRE(stmt.listValues.size() == 3);
    CHECK(stmt.listValues[0] == "Terminal");
    CHECK(stmt.listValues[1] == "iTerm2");
    CHECK(stmt.listValues[2] == "42");
}

TEST_CASE("blacklist without values fails") {
    Parser p{"blacklist = []"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("no process names were parsed"));
}

TEST_CASE("blacklist rejects invalid entries") {
    Parser p{R"(blacklist = ["Terminal", {])"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::ConfigPropertyStmt>(program.statements[0]);
    REQUIRE(stmt.listValues.size() == 1);
    CHECK(stmt.listValues[0] == "Terminal");
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("in blacklist"));
}

TEST_CASE("blacklist reports unexpected eof when closing bracket is missing") {
    Parser p{R"(blacklist = ["Terminal")"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::ConfigPropertyStmt>(program.statements[0]);
    REQUIRE(stmt.listValues.size() == 1);
    CHECK(stmt.listValues[0] == "Terminal");
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("while parsing blacklist"));
}

TEST_CASE("blacklist reports trailing tokens after closing bracket") {
    Parser p{R"(blacklist = ["Terminal"] extra)"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::ConfigPropertyStmt>(program.statements[0]);
    REQUIRE(stmt.listValues.size() == 1);
    CHECK(stmt.listValues[0] == "Terminal");
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("after blacklist config"));
    CHECK(p.errors()[0].message.contains("extra"));
}

TEST_CASE("brace expansion parses into brace key syntax") {
    Parser p{"cmd + {a, b} : echo hi"};
    auto program = p.parseProgram();

    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::HotkeyStmt>(program.statements[0]);
    REQUIRE(stmt.syntax.chords.size() == 1);
    REQUIRE(stmt.syntax.chords[0].key.has_value());
    const auto& key = *stmt.syntax.chords[0].key;
    CHECK(key.isBraceExpansion);
    REQUIRE(key.items.size() == 2);
    CHECK(key_char(key, 0).value == 'a');
    CHECK(key_char(key, 1).value == 'b');
}

TEST_CASE("empty brace expansion reports a dedicated error") {
    Parser p{"cmd + {} : echo hi"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("brace expansion must contain at least one key"));
}

TEST_CASE("brace expansion requires commas between items") {
    Parser p{"cmd + {a b} : echo hi"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("in brace expansion"));
    CHECK(p.errors()[0].message.contains("',' or '}'"));
}

TEST_CASE("brace expansion rejects invalid tokens inside braces") {
    Parser p{"cmd + {-} : echo hi"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("in brace expansion"));
}

TEST_CASE("brace expansion is rejected in remap target") {
    Parser p{"cmd + a | {b, c}"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("brace expansion is not allowed here"));
}

TEST_CASE("empty 0x literal triggers blank-hex error") {
    Parser p{"cmd + 0x : echo bad"};
    p.parseProgram();

    REQUIRE(!p.errors().empty());
    CHECK(p.errors()[0].message.contains("hex literal"));
}

TEST_CASE("hex > 0xFF triggers out-of-range error") {
    Parser p{"cmd + 0x1FF : echo bad"};
    p.parseProgram();

    REQUIRE(!p.errors().empty());
    CHECK(p.errors()[0].message.contains("out of range"));
}

TEST_CASE("malformed line produces only one error (recovery)") {
    Parser p{"cmd + 10 : echo bad"};
    p.parseProgram();

    CHECK(p.errors().size() == 1);
}

TEST_CASE("parseChord returns chord and reports trailing tokens on same line") {
    Parser p{"cmd + a extra"};
    auto chord = p.parseChord();

    REQUIRE(chord.has_value());
    REQUIRE(chord->modifiers.size() == 1);
    CHECK(builtin_modifier(chord->modifiers[0]) == BuiltinModifier::Cmd);
    REQUIRE(chord->key.has_value());
    CHECK(key_char(*chord->key).value == 'a');
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("after chord"));
    CHECK(p.errors()[0].message.contains("extra"));
}

TEST_CASE("parseChord on eof reports unexpected eof") {
    Parser p{""};
    auto chord = p.parseChord();

    CHECK_FALSE(chord.has_value());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("while parsing chord"));
}

TEST_CASE("parseChordSequence parses multiple chords") {
    Parser p{"cmd + a ; shift + b"};
    auto chords = p.parseChordSequence();

    REQUIRE(chords.has_value());
    REQUIRE(chords->size() == 2);
    REQUIRE((*chords)[0].key.has_value());
    REQUIRE((*chords)[1].key.has_value());
    CHECK(key_char(*(*chords)[0].key).value == 'a');
    CHECK(key_char(*(*chords)[1].key).value == 'b');
    REQUIRE((*chords)[0].modifiers.size() == 1);
    REQUIRE((*chords)[1].modifiers.size() == 1);
    CHECK(builtin_modifier((*chords)[0].modifiers[0]) == BuiltinModifier::Cmd);
    CHECK(builtin_modifier((*chords)[1].modifiers[0]) == BuiltinModifier::Shift);
    CHECK(p.errors().empty());
}

TEST_CASE("parseChordSequence rejects empty input") {
    Parser p{""};
    auto chords = p.parseChordSequence();

    CHECK_FALSE(chords.has_value());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("while parsing chord sequence"));
}

TEST_CASE("parseChordSequence rejects colon before first chord") {
    Parser p{": echo hi"};
    auto chords = p.parseChordSequence();

    CHECK_FALSE(chords.has_value());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("chord before ':' or '|'"));
}

TEST_CASE("parseChordSequence rejects pipe before first chord") {
    Parser p{"| shift + b"};
    auto chords = p.parseChordSequence();

    CHECK_FALSE(chords.has_value());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("chord before ':' or '|'"));
}

TEST_CASE("parseChordSequence rejects semicolon before first chord") {
    Parser p{"; shift + b"};
    auto chords = p.parseChordSequence();

    CHECK_FALSE(chords.has_value());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("chord before ';'"));
}

TEST_CASE("invalid leading character in config line is reported directly") {
    Parser p{"- a"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].row == 0);
    CHECK(p.errors()[0].col == 0);
    CHECK(p.errors()[0].message == "unexpected token Invalid ('-') while parsing chord sequence. Expected one of: Modifier, OpenBrace, Literal, Key, KeyHex, Integer");
}

TEST_CASE("whitespace-separated chords on one line require an explicit separator") {
    Parser p{"a f"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].row == 0);
    CHECK(p.errors()[0].col == 2);
    CHECK(p.errors()[0].message == "unexpected token Key ('f') while parsing chord sequence. Expected ';', ':' or '|'");
}

TEST_CASE("semicolon-separated chord sequence still requires a hotkey delimiter") {
    Parser p{"a ; 1"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message == "unexpected end of file after chord sequence. Expected ':' or '|'");
}

TEST_CASE("define_modifier parses builtin and custom parts") {
    Parser p{"define_modifier hyper = cmd + shift + meh"};
    auto program = p.parseProgram();

    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::DefineModifierStmt>(program.statements[0]);
    CHECK(stmt.name == "hyper");
    REQUIRE(stmt.parts.size() == 3);
    CHECK(builtin_modifier(stmt.parts[0]) == BuiltinModifier::Cmd);
    CHECK(builtin_modifier(stmt.parts[1]) == BuiltinModifier::Shift);
    CHECK(std::get<std::string>(stmt.parts[2].value) == "meh");
}

TEST_CASE("define_modifier rejects invalid custom modifier name") {
    Parser p{"define_modifier 10 = cmd"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("after 'define_modifier'"));
}

TEST_CASE("define_modifier requires equals after custom name") {
    Parser p{"define_modifier hyper cmd"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("expected Equals"));
}

TEST_CASE("define_modifier without expansions fails") {
    Parser p{"define_modifier hyper ="};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("no expansions found"));
}

TEST_CASE("hotkey requires a non-empty command after colon") {
    Parser p{"a :"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("expected Command after ':'"));
}

TEST_CASE("hotkey reports eof after flags with no chord") {
    Parser p{"cmd + a ^ & @"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("after chord sequence"));
}

TEST_CASE("leading flags before the first chord are rejected") {
    Parser p{"@ cmd + a : echo hi"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("while parsing chord sequence"));
}

TEST_CASE("flags between chords are rejected") {
    Parser p{"cmd + a ^ ; cmd + b : echo hi"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("while parsing hotkey"));
    CHECK(p.errors()[0].message.contains("':' or '|'"));
}

TEST_CASE("remap with pipe parses full source and target") {
    Parser p{"cmd + a | shift + b"};
    auto program = p.parseProgram();

    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::RemapStmt>(program.statements[0]);
    REQUIRE(stmt.source.chords.size() == 1);
    CHECK_FALSE(stmt.source.passthrough);
    CHECK_FALSE(stmt.source.repeat);
    CHECK_FALSE(stmt.source.onRelease);
    REQUIRE(stmt.source.chords[0].modifiers.size() == 1);
    CHECK(builtin_modifier(stmt.source.chords[0].modifiers[0]) == BuiltinModifier::Cmd);
    REQUIRE(stmt.source.chords[0].key.has_value());
    CHECK(key_char(*stmt.source.chords[0].key).value == 'a');
    REQUIRE(stmt.target.modifiers.size() == 1);
    CHECK(builtin_modifier(stmt.target.modifiers[0]) == BuiltinModifier::Shift);
    REQUIRE(stmt.target.key.has_value());
    CHECK(key_char(*stmt.target.key).value == 'b');
}

TEST_CASE("remap rejects missing target chord") {
    Parser p{"cmd + a |"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.empty());
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("chord is missing a key"));
}

TEST_CASE("remap reports trailing tokens after target chord") {
    Parser p{"cmd + a | shift + b extra"};
    auto program = p.parseProgram();

    REQUIRE(program.statements.size() == 1);
    CHECK(std::holds_alternative<ast::RemapStmt>(program.statements[0]));
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("after remap target"));
    CHECK(p.errors()[0].message.contains("extra"));
}
