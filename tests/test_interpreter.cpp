#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "doctest.h"
#include "input/keysym.hpp"
#include "input/locale.hpp"
#include "input/modifier.hpp"
#include "lang/config_loader.hpp"
#include "lang/interpreter.hpp"
#include "lang/parser.hpp"
#include "runtime/hotkey_engine.hpp"

namespace {

InterpreterResult interpret_source(std::string_view src) {
    [[maybe_unused]] static const bool _ = initializeKeycodeMap();
    Parser p{std::string{src}};
    auto program = p.parseProgram();
    return interpretProgram(program);
}

// bindings whose action is a command
std::vector<Binding> hotkey_bindings(const std::vector<Binding>& bindings) {
    std::vector<Binding> out;
    for (const auto& b : bindings) {
        if (std::holds_alternative<std::string>(b.action)) out.push_back(b);
    }
    return out;
}

// bindings whose action is a remap target chord
std::vector<Binding> remap_bindings(const std::vector<Binding>& bindings) {
    std::vector<Binding> out;
    for (const auto& b : bindings) {
        if (std::holds_alternative<Chord>(b.action)) out.push_back(b);
    }
    return out;
}

std::set<uint32_t> keycodes_of(const std::vector<Binding>& bindings) {
    std::set<uint32_t> out;
    for (const auto& b : bindings) {
        REQUIRE(b.source.chords.size() == 1);
        out.insert(b.source.chords[0].keysym.keycode);
    }
    return out;
}

}  // namespace

TEST_CASE("simple hotkey: keycode, modifier flag, command all set correctly") {
    auto r = interpret_source("cmd + a : echo hi");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);

    const auto& hk = hotkeys[0].source;
    const auto& cmd = std::get<std::string>(hotkeys[0].action);
    REQUIRE(hk.chords.size() == 1);
    CHECK(hk.chords[0].keysym.keycode == getKeycode('a'));
    CHECK(hk.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    CHECK_FALSE(hk.passthrough);
    CHECK_FALSE(hk.repeat);
    CHECK_FALSE(hk.on_release);
    CHECK(cmd == "echo hi");
}

TEST_CASE("trackpad_fingers(N) is carried onto the runtime chord") {
    auto r = interpret_source("trackpad_fingers(2) + cmd + j : echo hi");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    const auto& chord = hotkeys[0].source.chords[0];
    REQUIRE(chord.fingerCount.has_value());
    CHECK(*chord.fingerCount == 2);
    CHECK(chord.modifiers.flags == Hotkey_Flag_Cmd);
}

TEST_CASE("a hotkey without trackpad_fingers(N) has no finger requirement") {
    auto r = interpret_source("cmd + j : echo hi");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    CHECK_FALSE(hotkeys[0].source.chords[0].fingerCount.has_value());
}

TEST_CASE("trackpad_fingers(N) works on a remap source") {
    auto r = interpret_source("trackpad_fingers(2) + a | b");
    REQUIRE(r.errors.empty());
    const auto remaps = remap_bindings(r.bindings);
    REQUIRE(remaps.size() == 1);
    REQUIRE(remaps[0].source.chords[0].fingerCount.has_value());
    CHECK(*remaps[0].source.chords[0].fingerCount == 2);
}

TEST_CASE("finger-count requirement matches only the live count") {
    const Chord binding{.keysym = {.keycode = 5}, .modifiers = {.flags = Hotkey_Flag_Cmd}, .fingerCount = 2};
    const Chord ev{.keysym = {.keycode = 5}, .modifiers = {.flags = Hotkey_Flag_Cmd}};
    CHECK(binding.isActivatedBy(ev, 2));
    CHECK_FALSE(binding.isActivatedBy(ev, 1));
}

TEST_CASE("no finger requirement matches any live count") {
    const Chord binding{.keysym = {.keycode = 5}, .modifiers = {.flags = Hotkey_Flag_Cmd}};
    const Chord ev{.keysym = {.keycode = 5}, .modifiers = {.flags = Hotkey_Flag_Cmd}};
    CHECK(binding.isActivatedBy(ev, 3));
    CHECK(binding.isActivatedBy(ev, 0));
}

TEST_CASE("trackpad_fingers(0) matches only when nothing is touching") {
    const Chord binding{.keysym = {.keycode = 5}, .modifiers = {.flags = 0}, .fingerCount = 0};
    const Chord ev{.keysym = {.keycode = 5}, .modifiers = {.flags = 0}};
    CHECK(binding.isActivatedBy(ev, 0));
    CHECK_FALSE(binding.isActivatedBy(ev, 1));
}

TEST_CASE("the event chord carries no finger count (so it never displays one)") {
    const Chord ev{.keysym = {.keycode = 5}, .modifiers = {.flags = Hotkey_Flag_Cmd}};
    CHECK_FALSE(ev.fingerCount.has_value());
}

TEST_CASE("corner_size and tap_timeout config knobs parse") {
    auto r = interpret_source("corner_size = 20\ntap_timeout = 250\n");
    REQUIRE(r.errors.empty());
    CHECK(r.config.cornerSize == 20);
    CHECK(r.config.tapTimeout == std::chrono::milliseconds(250));
}

TEST_CASE("corner_size out of range is an error") {
    auto r = interpret_source("corner_size = 60\n");
    CHECK_FALSE(r.errors.empty());
}

TEST_CASE("trackpad_tap lowers to a tap binding, not a keyboard binding") {
    auto r = interpret_source("cmd + trackpad_tap(tr) : echo hi");
    REQUIRE(r.errors.empty());
    CHECK(r.bindings.empty());
    REQUIRE(r.tapBindings.size() == 1);
    CHECK(r.tapBindings[0].zone == Zone::TopRight);
    CHECK(r.tapBindings[0].modifiers.flags == Hotkey_Flag_Cmd);
    CHECK(std::get<std::string>(r.tapBindings[0].action) == "echo hi");
}

TEST_CASE("a bare trackpad_tap with no modifier is an error") {
    auto r = interpret_source("trackpad_tap(tr) : echo hi");
    CHECK_FALSE(r.errors.empty());
    CHECK(r.tapBindings.empty());
}

TEST_CASE("a trackpad_tap remap lowers to a tap binding with a target chord") {
    auto r = interpret_source("cmd + trackpad_tap(bl) | a");
    REQUIRE(r.errors.empty());
    REQUIRE(r.tapBindings.size() == 1);
    CHECK(r.tapBindings[0].zone == Zone::BottomLeft);
    CHECK(std::holds_alternative<Chord>(r.tapBindings[0].action));
}

TEST_CASE("a trackpad_tap remap to a media key is allowed") {
    auto r = interpret_source("cmd + trackpad_tap(tr) | sound_up");
    REQUIRE(r.errors.empty());
    REQUIRE(r.tapBindings.size() == 1);
    REQUIRE(std::holds_alternative<Chord>(r.tapBindings[0].action));
    CHECK(std::get<Chord>(r.tapBindings[0].action).modifiers.has(Hotkey_Flag_NX));
}

TEST_CASE("a keyboard remap to a media key is allowed") {
    auto r = interpret_source("cmd + m | mute");
    REQUIRE(r.errors.empty());
    const auto remaps = remap_bindings(r.bindings);
    REQUIRE(remaps.size() == 1);
    CHECK(std::get<Chord>(remaps[0].action).modifiers.has(Hotkey_Flag_NX));
}

TEST_CASE("engine matches a corner tap by zone and modifiers") {
    auto r = interpret_source("cmd + trackpad_tap(tr) : echo hi");
    REQUIRE(r.errors.empty());
    HotkeyEngine engine;
    engine.applyConfig({}, r.tapBindings, r.config);
    CHECK(engine.hasTapBinding(Zone::TopRight, ModifierFlags{.flags = Hotkey_Flag_Cmd}));
    CHECK_FALSE(engine.hasTapBinding(Zone::TopRight, ModifierFlags{.flags = Hotkey_Flag_Alt}));
    CHECK_FALSE(engine.hasTapBinding(Zone::BottomLeft, ModifierFlags{.flags = Hotkey_Flag_Cmd}));
    CHECK_FALSE(engine.handleTap(Zone::BottomRight, ModifierFlags{.flags = Hotkey_Flag_Cmd}));
}

TEST_CASE("multiple modifiers combine flags") {
    auto r = interpret_source("cmd + shift + alt + a : noop");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    const auto& hk = hotkeys[0].source;
    CHECK(hk.chords[0].modifiers.flags == (Hotkey_Flag_Cmd | Hotkey_Flag_Shift | Hotkey_Flag_Alt));
}

TEST_CASE("brace key expansion: each expansion gets the right keycode and shared command") {
    auto r = interpret_source("cmd + {a, b, c} : echo one");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 3);

    const auto keycodes = keycodes_of(hotkeys);
    CHECK(keycodes == std::set<uint32_t>{getKeycode('a'), getKeycode('b'), getKeycode('c')});

    // every expansion shares the same command and modifier set
    for (const auto& b : hotkeys) {
        CHECK(std::get<std::string>(b.action) == "echo one");
        CHECK(b.source.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    }
}

TEST_CASE("brace key + brace command: pairs by index") {
    auto r = interpret_source("cmd + {a, b} : echo {one, two}");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 2);

    bool saw_a = false;
    bool saw_b = false;
    for (const auto& b : hotkeys) {
        const auto& cmd = std::get<std::string>(b.action);
        if (b.source.chords[0].keysym.keycode == getKeycode('a')) {
            CHECK(cmd == "echo one");
            saw_a = true;
        } else if (b.source.chords[0].keysym.keycode == getKeycode('b')) {
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
    CHECK(hotkey_bindings(r.bindings).empty());
}

TEST_CASE("second unescaped brace group in command: error, no hotkey emitted") {
    auto r = interpret_source("cmd + a : echo {one, two} and {three, four}");
    REQUIRE(!r.errors.empty());
    CHECK(r.errors[0].message.contains("supports only one"));
    CHECK(hotkey_bindings(r.bindings).empty());
}

TEST_CASE("escaped braces in command are literal and do not trigger expansion") {
    auto r = interpret_source("cmd + a : echo {{literal}}");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    CHECK(std::get<std::string>(hotkeys[0].action) == "echo {literal}");
}

TEST_CASE("a comma inside escaped braces stays literal, not split") {
    auto r = interpret_source("cmd + a : echo {{one,two}}");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    CHECK(std::get<std::string>(hotkeys[0].action) == "echo {one,two}");
}

TEST_CASE("escaped command braces coexist with a key brace expansion") {
    auto r = interpret_source("cmd + {a,b} : echo {{lit}}");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 2);
    CHECK(std::get<std::string>(hotkeys[0].action) == "echo {lit}");
    CHECK(std::get<std::string>(hotkeys[1].action) == "echo {lit}");
}

TEST_CASE("brace expansion in more than one sequence chord is an error") {
    auto r = interpret_source("cmd + {a,b} ; cmd + {c,d} : echo");
    REQUIRE(!r.errors.empty());
    CHECK(r.errors[0].message.contains("only one chord"));
    CHECK(hotkey_bindings(r.bindings).empty());
}

TEST_CASE("an empty command does not swallow the following line") {
    Parser p{"cmd + a :\ncmd + b : echo hi"};
    auto program = p.parseProgram();
    REQUIRE(p.errors().size() == 1);
    REQUIRE(program.statements.size() == 1);
    auto r = interpretProgram(program);
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    CHECK(std::get<std::string>(hotkeys[0].action) == "echo hi");
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

TEST_CASE("unknown config-like assignment is rejected during parsing") {
    auto r = ConfigLoader::loadFromContents("nonexistent_property = 1");
    REQUIRE(!r.parseErrors.empty());
    const auto& msg = r.parseErrors[0].message;
    CHECK(msg.contains("chord is missing a key"));
}

TEST_CASE("unknown modifier reports the offending name") {
    auto r = interpret_source("nonexistent + a : echo hi");
    REQUIRE(!r.errors.empty());
    const auto& msg = r.errors[0].message;
    CHECK(msg.contains("unknown modifier"));
    CHECK(msg.contains("nonexistent"));
    CHECK(hotkey_bindings(r.bindings).empty());
}

TEST_CASE("custom modifier resolves to the union of its parts") {
    auto r = interpret_source(
        "define_modifier hyper = cmd + shift + alt + ctrl\n"
        "hyper + a : noop");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    const auto& hk = hotkeys[0].source;
    CHECK(hk.chords[0].modifiers.flags
          == (Hotkey_Flag_Cmd | Hotkey_Flag_Shift | Hotkey_Flag_Alt | Hotkey_Flag_Control));
}

TEST_CASE("custom modifier may reference another custom modifier") {
    auto r = interpret_source(
        "define_modifier meh = ctrl + alt + shift\n"
        "define_modifier hyper = meh + cmd\n"
        "hyper + x : noop");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    const auto flags = hotkeys[0].source.chords[0].modifiers.flags;
    CHECK(flags == (Hotkey_Flag_Cmd | Hotkey_Flag_Shift | Hotkey_Flag_Alt | Hotkey_Flag_Control));
}

TEST_CASE("remap produces source hotkey and target chord with correct flags/keycodes") {
    auto r = interpret_source("cmd + a | shift + b");
    REQUIRE(r.errors.empty());
    const auto remaps = remap_bindings(r.bindings);
    REQUIRE(remaps.size() == 1);
    const auto& remap = remaps[0];
    REQUIRE(remap.source.chords.size() == 1);
    CHECK(remap.source.chords[0].keysym.keycode == getKeycode('a'));
    CHECK(remap.source.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    const auto& target = std::get<Chord>(remap.action);
    CHECK(target.keysym.keycode == getKeycode('b'));
    CHECK(target.modifiers.flags == Hotkey_Flag_Shift);
}

TEST_CASE("remap with flags (~/&/^) is rejected") {
    auto r = interpret_source("cmd + a ~ | shift + b");
    REQUIRE(!r.errors.empty());
    CHECK(r.errors[0].message.contains("remaps do not support"));
    CHECK(remap_bindings(r.bindings).empty());
}

TEST_CASE("blacklist preserves order and lowercases strings") {
    auto r = interpret_source(R"(blacklist = ["Terminal" "iTerm2" "Code" "a b c"])");
    REQUIRE(r.errors.empty());
    REQUIRE(r.config.blacklist.size() == 4);
    CHECK(r.config.blacklist[0] == "terminal");
    CHECK(r.config.blacklist[1] == "iterm2");
    CHECK(r.config.blacklist[2] == "code");
    CHECK(r.config.blacklist[3] == "a b c");
}

TEST_CASE("passthrough/repeat/release flags are propagated to Hotkey") {
    auto r = interpret_source(
        "cmd + a ~ : passthrough\n"
        "cmd + b & : repeat\n"
        "cmd + c ^ : release");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 3);

    bool saw_passthrough = false;
    bool saw_repeat = false;
    bool saw_release = false;
    for (const auto& b : hotkeys) {
        const auto& hk = b.source;
        const auto& cmd = std::get<std::string>(b.action);
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
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    const auto& hk = hotkeys[0].source;
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
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    CHECK(hotkeys[0].source.chords[0].keysym.keycode == 0x7B);
}

TEST_CASE("multi-chord sequence preserves chord order") {
    auto r = interpret_source("cmd + a ; cmd + b : noop");
    REQUIRE(r.errors.empty());
    const auto hotkeys = hotkey_bindings(r.bindings);
    REQUIRE(hotkeys.size() == 1);
    const auto& hk = hotkeys[0].source;
    REQUIRE(hk.chords.size() == 2);
    CHECK(hk.chords[0].keysym.keycode == getKeycode('a'));
    CHECK(hk.chords[1].keysym.keycode == getKeycode('b'));
    CHECK(hk.chords[0].modifiers.flags == Hotkey_Flag_Cmd);
    CHECK(hk.chords[1].modifiers.flags == Hotkey_Flag_Cmd);
}
