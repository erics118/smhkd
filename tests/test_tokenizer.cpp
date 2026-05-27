#include <string_view>
#include <vector>

#include "doctest.h"
#include "lang/parser.hpp"
#include "lang/tokenizer.hpp"

namespace {

std::vector<Token> tokenize_all(std::string_view input) {
    Tokenizer tk{input};
    std::vector<Token> out;
    while (true) {
        Token t = tk.next();
        if (t.type == TokenType::EndOfFile) break;
        out.push_back(std::move(t));
    }
    return out;
}

}  // namespace

TEST_CASE("integers tokenize as Integer, not Modifier/Key") {
    auto toks = tokenize_all("max_chord_interval = 500");
    REQUIRE(toks.size() == 3);
    CHECK(toks[0].type == TokenType::ConfigProperty);
    CHECK(toks[1].type == TokenType::Equals);
    CHECK(toks[2].type == TokenType::Integer);
    CHECK(toks[2].text == "500");
}

TEST_CASE("single-digit identifier is Integer") {
    auto toks = tokenize_all("1");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].type == TokenType::Integer);
}

TEST_CASE("single-letter identifier is Key") {
    auto toks = tokenize_all("a");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].type == TokenType::Key);
}

TEST_CASE("multi-letter identifier is Modifier (not literal/config)") {
    auto toks = tokenize_all("hyper");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].type == TokenType::Modifier);
}

TEST_CASE("f1 is recognized as Literal") {
    auto toks = tokenize_all("f1");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].type == TokenType::Literal);
}

TEST_CASE("unknown characters like '-' are not allowed") {
    Parser p{"cmd - 1"};
    auto program = p.parseProgram();
    REQUIRE(!p.errors().empty());
    CHECK(p.errors()[0].message
              == "unexpected token Invalid ('-') while parsing chord sequence. Expected one of: Modifier, OpenBrace, Literal, Key, KeyHex, Integer");
    CHECK(program.statements.empty());
}

TEST_CASE("hex literals tokenize as KeyHex") {
    auto toks = tokenize_all("0xFF");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].type == TokenType::KeyHex);
    CHECK(toks[0].text == "FF");
}

TEST_CASE("0x with no digits produces empty KeyHex") {
    auto toks = tokenize_all("0x");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].type == TokenType::KeyHex);
    CHECK(toks[0].text.empty());
}

TEST_CASE("colon switches tokenizer into command mode") {
    auto toks = tokenize_all("a : echo hi");
    REQUIRE(toks.size() == 3);
    CHECK(toks[0].type == TokenType::Key);
    CHECK(toks[1].type == TokenType::Colon);
    CHECK(toks[2].type == TokenType::Command);
    CHECK(toks[2].text == "echo hi");
}
