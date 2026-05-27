#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "doctest.h"
#include "input/keysym.hpp"
#include "input/locale.hpp"
#include "input/modifier.hpp"
#include "lang/interpreter.hpp"
#include "lang/parser.hpp"

namespace {

InterpreterResult interpret_source(std::string_view src) {
    [[maybe_unused]] static const bool _ = initializeKeycodeMap();
    Parser p{std::string{src}};
    auto program = p.parseProgram();
    Interpreter interp;
    return interp.interpret(program);
}

// keycodes/commands in a hotkey map, indexed for easier assertion
std::vector<std::pair<Hotkey, std::string>> as_vector(const std::map<Hotkey, std::string>& m) {
    return {m.begin(), m.end()};
}

std::set<uint32_t> keycodes_of(const std::map<Hotkey, std::string>& m) {
    std::set<uint32_t> out;
    for (const auto& [hk, _] : m) {
        REQUIRE(hk.chords.size() == 1);
        out.insert(hk.chords[0].keysym.keycode);
    }
    return out;
}

std::set<std::string> commands_of(const std::map<Hotkey, std::string>& m) {
    std::set<std::string> out;
    for (const auto& [_, cmd] : m) out.insert(cmd);
    return out;
}

}  // namespace

TEST_CASE("simple hotkey: keycode, modifier flag, command all set correctly") {
    auto r = interpret_source("cmd + a : echo hi");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 1);

    const auto& [hk, cmd] = *r.hotkeys.begin();
    REQUIRE(hk.chords.size() == 1);
    CHECK(hk.chords[0].keysym.keycode == getKeycode('a'));
    CHECK(hk.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    CHECK_FALSE(hk.passthrough);
    CHECK_FALSE(hk.repeat);
    CHECK_FALSE(hk.on_release);
    CHECK(cmd == "echo hi");
}

TEST_CASE("multiple modifiers combine flags") {
    auto r = interpret_source("cmd + shift + alt + a : noop");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 1);
    const auto& hk = r.hotkeys.begin()->first;
    CHECK(hk.chords[0].modifiers.flags == (Hotkey_Flag_Cmd | Hotkey_Flag_Shift | Hotkey_Flag_Alt));
}

TEST_CASE("brace key expansion: each expansion gets the right keycode and shared command") {
    auto r = interpret_source("cmd + {a, b, c} : echo one");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 3);

    const auto keycodes = keycodes_of(r.hotkeys);
    CHECK(keycodes == std::set<uint32_t>{getKeycode('a'), getKeycode('b'), getKeycode('c')});

    // every expansion shares the same command and modifier set
    for (const auto& [hk, cmd] : r.hotkeys) {
        CHECK(cmd == "echo one");
        CHECK(hk.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    }
}

TEST_CASE("brace key + brace command: pairs by index") {
    auto r = interpret_source("cmd + {a, b} : echo {one, two}");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 2);

    // map sorts by Hotkey (modifiers, then keycode). Look up by keycode.
    bool saw_a = false;
    bool saw_b = false;
    for (const auto& [hk, cmd] : r.hotkeys) {
        if (hk.chords[0].keysym.keycode == getKeycode('a')) {
            CHECK(cmd == "echo one");
            saw_a = true;
        } else if (hk.chords[0].keysym.keycode == getKeycode('b')) {
            CHECK(cmd == "echo two");
            saw_b = true;
        }
    }
    CHECK(saw_a);
    CHECK(saw_b);
}

TEST_CASE("mismatched brace counts: error mentions both counts and produces no hotkey") {
    auto r = interpret_source("cmd + {a, b, c} : echo {one, two}");
    REQUIRE(r.errors.size() == 1);
    const auto& msg = r.errors[0].message;
    CHECK(msg.contains("brace expansion mismatch"));
    CHECK(msg.contains('3'));
    CHECK(msg.contains('2'));
    CHECK(r.hotkeys.empty());
}

TEST_CASE("second unescaped brace group in command: error, no hotkey emitted") {
    auto r = interpret_source("cmd + a : echo {one, two} and {three, four}");
    REQUIRE(!r.errors.empty());
    CHECK(r.errors[0].message.contains("supports only one"));
    CHECK(r.hotkeys.empty());
}

TEST_CASE("escaped braces in command are literal and do not trigger expansion") {
    auto r = interpret_source("cmd + a : echo {{literal}}");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 1);
    CHECK(r.hotkeys.begin()->second == "echo {literal}");
}

TEST_CASE("config values apply to the correct field") {
    auto r = interpret_source(
        "max_chord_interval = 1234\n"
        "hold_modifier_threshold = 250\n"
        "simultaneous_threshold = 75");
    CHECK(r.errors.empty());
    CHECK(r.config.maxChordInterval == std::chrono::milliseconds(1234));
    CHECK(r.config.holdModifierThreshold == std::chrono::milliseconds(250));
    CHECK(r.config.simultaneousThreshold == std::chrono::milliseconds(75));
}

TEST_CASE("unknown config property is reported by name") {
    auto r = interpret_source("nonexistent_property = 1");
    REQUIRE(!r.errors.empty());
    const auto& msg = r.errors[0].message;
    CHECK(msg.contains("unknown config property"));
    CHECK(msg.contains("nonexistent_property"));
}

TEST_CASE("unknown modifier reports the offending name") {
    auto r = interpret_source("nonexistent + a : echo hi");
    REQUIRE(!r.errors.empty());
    const auto& msg = r.errors[0].message;
    CHECK(msg.contains("unknown modifier"));
    CHECK(msg.contains("nonexistent"));
    CHECK(r.hotkeys.empty());
}

TEST_CASE("custom modifier resolves to the union of its parts") {
    auto r = interpret_source(
        "define_modifier hyper = cmd + shift + alt + ctrl\n"
        "hyper + a : noop");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 1);
    const auto& hk = r.hotkeys.begin()->first;
    CHECK(hk.chords[0].modifiers.flags
          == (Hotkey_Flag_Cmd | Hotkey_Flag_Shift | Hotkey_Flag_Alt | Hotkey_Flag_Control));
}

TEST_CASE("custom modifier may reference another custom modifier") {
    auto r = interpret_source(
        "define_modifier meh = ctrl + alt + shift\n"
        "define_modifier hyper = meh + cmd\n"
        "hyper + x : noop");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 1);
    const auto flags = r.hotkeys.begin()->first.chords[0].modifiers.flags;
    CHECK(flags == (Hotkey_Flag_Cmd | Hotkey_Flag_Shift | Hotkey_Flag_Alt | Hotkey_Flag_Control));
}

TEST_CASE("remap produces source hotkey and target chord with correct flags/keycodes") {
    auto r = interpret_source("cmd + a | shift + b");
    REQUIRE(r.errors.empty());
    REQUIRE(r.remaps.size() == 1);
    const auto& remap = r.remaps[0];
    REQUIRE(remap.source.chords.size() == 1);
    CHECK(remap.source.chords[0].keysym.keycode == getKeycode('a'));
    CHECK(remap.source.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    CHECK(remap.target.keysym.keycode == getKeycode('b'));
    CHECK(remap.target.modifiers.flags == Hotkey_Flag_Shift);
}

TEST_CASE("remap with flags (@/&/^) is rejected") {
    auto r = interpret_source("cmd + a @ | shift + b");
    REQUIRE(!r.errors.empty());
    CHECK(r.errors[0].message.contains("remaps do not support"));
    CHECK(r.remaps.empty());
}

TEST_CASE("blacklist preserves order and exact strings") {
    auto r = interpret_source(R"(blacklist = ["Terminal", "iTerm2", "Code"])");
    REQUIRE(r.errors.empty());
    REQUIRE(r.config.blacklist.size() == 3);
    CHECK(r.config.blacklist[0] == "Terminal");
    CHECK(r.config.blacklist[1] == "iTerm2");
    CHECK(r.config.blacklist[2] == "Code");
}

TEST_CASE("passthrough/repeat/release flags are propagated to Hotkey") {
    auto r = interpret_source(
        "cmd + a @ : passthrough\n"
        "cmd + b & : repeat\n"
        "cmd + c ^ : release");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 3);

    bool saw_passthrough = false;
    bool saw_repeat = false;
    bool saw_release = false;
    for (const auto& [hk, cmd] : r.hotkeys) {
        if (cmd == "passthrough") {
            CHECK(hk.passthrough);
            CHECK_FALSE(hk.repeat);
            CHECK_FALSE(hk.on_release);
            saw_passthrough = true;
        } else if (cmd == "repeat") {
            CHECK(hk.repeat);
            CHECK_FALSE(hk.passthrough);
            CHECK_FALSE(hk.on_release);
            saw_repeat = true;
        } else if (cmd == "release") {
            CHECK(hk.on_release);
            CHECK_FALSE(hk.passthrough);
            CHECK_FALSE(hk.repeat);
            saw_release = true;
        }
    }
    CHECK(saw_passthrough);
    CHECK(saw_repeat);
    CHECK(saw_release);
}

TEST_CASE("flags apply only after the final chord in a sequence") {
    auto r = interpret_source("cmd + a ; cmd + b ^ & : noop");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 1);
    const auto& hk = r.hotkeys.begin()->first;
    REQUIRE(hk.chords.size() == 2);
    CHECK(hk.repeat);
    CHECK(hk.on_release);
    CHECK_FALSE(hk.passthrough);
    CHECK(hk.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    CHECK(hk.chords[1].modifiers.flags == Hotkey_Flag_Cmd);
}

TEST_CASE("hex keycode maps directly into the chord") {
    auto r = interpret_source("cmd + 0x7B : noop");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 1);
    CHECK(r.hotkeys.begin()->first.chords[0].keysym.keycode == 0x7B);
}

TEST_CASE("multi-chord sequence preserves chord order") {
    auto r = interpret_source("cmd + a ; cmd + b : noop");
    REQUIRE(r.errors.empty());
    REQUIRE(r.hotkeys.size() == 1);
    const auto& hk = r.hotkeys.begin()->first;
    REQUIRE(hk.chords.size() == 2);
    CHECK(hk.chords[0].keysym.keycode == getKeycode('a'));
    CHECK(hk.chords[1].keysym.keycode == getKeycode('b'));
    CHECK(hk.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    CHECK(hk.chords[1].modifiers.flags == Hotkey_Flag_Cmd);
}
