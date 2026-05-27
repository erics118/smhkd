#include "doctest.h"
#include "lang/parser.hpp"

TEST_CASE("simple hotkey parses to one HotkeyStmt") {
    Parser p{"cmd + a : echo hi"};
    auto program = p.parseProgram();
    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    CHECK(std::holds_alternative<ast::HotkeyStmt>(program.statements[0]));
}

TEST_CASE("multi-digit integer in key position emits error and recovers") {
    Parser p{"cmd + 10 : echo bad\ncmd + 1 : echo good"};
    auto program = p.parseProgram();
    REQUIRE(p.errors().size() == 1);
    CHECK(p.errors()[0].message.contains("'10' is not a valid key"));
    // bad line dropped, good line parsed
    REQUIRE(program.statements.size() == 1);
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
}

TEST_CASE("scalar config rejects non-integer value") {
    Parser p{"max_chord_interval = abc"};
    auto program = p.parseProgram();
    REQUIRE(!p.errors().empty());
    CHECK(p.errors()[0].message.contains("expected integer"));
}

TEST_CASE("blacklist parses string list") {
    Parser p{R"(blacklist = ["Terminal", "iTerm2"])"};
    auto program = p.parseProgram();
    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::ConfigPropertyStmt>(program.statements[0]);
    CHECK(stmt.listValues.size() == 2);
    CHECK(stmt.listValues[0] == "Terminal");
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

TEST_CASE("define_modifier parses to DefineModifierStmt") {
    Parser p{"define_modifier hyper = cmd + shift"};
    auto program = p.parseProgram();
    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    auto& stmt = std::get<ast::DefineModifierStmt>(program.statements[0]);
    CHECK(stmt.name == "hyper");
    CHECK(stmt.parts.size() == 2);
}

TEST_CASE("remap with pipe parses to RemapStmt") {
    Parser p{"cmd + a | shift + b"};
    auto program = p.parseProgram();
    CHECK(p.errors().empty());
    REQUIRE(program.statements.size() == 1);
    CHECK(std::holds_alternative<ast::RemapStmt>(program.statements[0]));
}
