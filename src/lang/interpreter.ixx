module;

#include <algorithm>
#include <chrono>
#include <map>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

export module interpreter;
import ast;
import chord;
import hotkey;
import locale;
import keysym;
import modifier;
import log;

export struct ConfigProperties {
    // max time between chord presses
    std::chrono::milliseconds maxChordInterval{3000};

    // minimum time for a keysym to be held to be considered as a hold_modifier
    // otherwise it's considered a normal keysym event
    std::chrono::milliseconds holdModifierThreshold{500};

    // max time between keysyms to be considered as simultaneous
    std::chrono::milliseconds simultaneousThreshold{50};

    // process names to ignore (case-insensitive)
    std::vector<std::string> blacklist;
};

export struct InterpreterResult {
    std::map<Hotkey, std::string> hotkeys;
    ConfigProperties config;
};

export class Interpreter {
   public:
    Interpreter() = default;
    [[nodiscard]] InterpreterResult interpret(const ast::Program& program);

   private:
    std::unordered_map<std::string, std::vector<ast::ModifierAtom>> defines;
    std::unordered_map<std::string, int> cache;

    // modifier resolution
    int resolveModifierFlags(const std::string& name);

    // hotkey building
    void setChordKeyFromAtom(Chord& chord, const ast::KeyAtom& atom);
    Hotkey buildBaseHotkey(const ast::HotkeySyntax& syn);
    void setHotkeyKeys(Hotkey& hk, const ast::HotkeySyntax& syn, int braceChordIndex, size_t braceItemIndex);

    // command parsing
    static std::string trim(std::string_view s);
    static std::string unescapeDoubleBraces(std::string_view s);
    std::vector<std::string> parseCommandBraceExpansion(const std::string& command);
};

// resolves modifier name to its flag value
// handles builtin modifiers and custom modifiers
// modifiers must be defined before usage.
int Interpreter::resolveModifierFlags(const std::string& name) {
    if (auto it = cache.find(name); it != cache.end()) return it->second;
    if (auto bi = parseBuiltinModifier(name)) return cache[name] = builtinModifierToFlags(*bi);
    if (auto it = defines.find(name); it != defines.end()) {
        int flags = 0;
        for (const auto& part : it->second) {
            int partFlags = std::holds_alternative<BuiltinModifier>(part.value)
                              ? builtinModifierToFlags(std::get<BuiltinModifier>(part.value))
                              : resolveModifierFlags(std::get<std::string>(part.value));
            if (partFlags == 0) {
                warn("invalid modifier reference in custom modifier definition '{}'", name);
                return cache[name] = 0;
            }
            flags |= partFlags;
        }
        return cache[name] = flags;
    }
    warn("unknown modifier '{}'", name);
    return cache[name] = 0;
}

// Sets the chord's keycode from an AST key atom (LiteralKey or KeyChar)
void Interpreter::setChordKeyFromAtom(Chord& chord, const ast::KeyAtom& atom) {
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, LiteralKey>) {
            chord.keysym.keycode = literalKeyToKeycode(v);
            chord.modifiers.flags |= getImplicitFlags(v);
        } else if constexpr (std::is_same_v<T, ast::KeyChar>) {
            if (v.isHex) {
                chord.keysym.keycode = static_cast<uint32_t>(static_cast<unsigned char>(v.value));
            } else {
                chord.keysym.keycode = getKeycode(v.value);
            }
        }
    },
        atom.value);
}

// build a base hotkey with the modifiers, without the keysyms
// because keysyms are handled during brace expansion
// TODO: allow for brace expansions to include modifiers
// returns a hotkey with empty chords if any modifier is invalid
Hotkey Interpreter::buildBaseHotkey(const ast::HotkeySyntax& syn) {
    Hotkey base{.passthrough = syn.passthrough, .repeat = syn.repeat, .on_release = syn.onRelease};
    base.chords.reserve(syn.chords.size());
    for (const auto& chordSyn : syn.chords) {
        Chord c{.modifiers = {.flags = 0}};
        for (const auto& modName : chordSyn.modifiers) {
            int flags = 0;
            if (std::holds_alternative<BuiltinModifier>(modName.value)) {
                flags = builtinModifierToFlags(std::get<BuiltinModifier>(modName.value));
            } else {
                flags = resolveModifierFlags(std::get<std::string>(modName.value));
                // if a non-builtin modifier resolves to 0, it's an invalid modifier
                if (flags == 0) {
                    // already warned, so just return an empty hotkey
                    return Hotkey{};
                }
            }
            c.modifiers.flags |= flags;
        }
        base.chords.push_back(c);
    }
    return base;
}

// sets the keysyms for chords in a hotkey
void Interpreter::setHotkeyKeys(Hotkey& hk, const ast::HotkeySyntax& syn, int braceChordIndex, size_t braceItemIndex) {
    for (size_t i = 0; i < syn.chords.size(); i++) {
        const auto& key = syn.chords[i].key;
        if (!key.has_value() || key->items.empty()) {
            warn("chord {} in multi-chord sequence is missing a keysym", i + 1);
            // mark hotkey as invalid by clearing chords
            hk.chords.clear();
            return;
        }
        const auto& keyAtom = (i == braceChordIndex && braceItemIndex < key->items.size())
                                ? key->items[braceItemIndex]
                                : key->items.front();

        setChordKeyFromAtom(hk.chords[i], keyAtom);
    }
}

// trims leading and trailing whitespace from a string
std::string Interpreter::trim(std::string_view s) {
    auto start = std::ranges::find_if_not(s, [](unsigned char c) { return std::isspace(c); });
    if (start == s.end()) return "";
    auto end = std::find_if_not(s.rbegin(), std::make_reverse_iterator(start), [](unsigned char c) { return std::isspace(c); }).base();
    return std::string(start, end);
}

// converts double braces to single braces: {{ -> { and }} -> }
// used to escape literal braces in command strings
std::string Interpreter::unescapeDoubleBraces(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (i + 1 < s.size() && s[i] == s[i + 1] && (s[i] == '{' || s[i] == '}')) {
            result += s[i];
            i++;  // skip the second brace
        } else {
            result += s[i];
        }
    }
    return result;
}

// parses brace expansion in command strings.
// returns a vector of the expanded commands
std::vector<std::string> Interpreter::parseCommandBraceExpansion(const std::string& command) {
    // find start brace
    size_t braceStart = std::string::npos;
    for (size_t i = 0; i < command.size(); i++) {
        if (command[i] == '{' && (i + 1 >= command.size() || command[i + 1] != '{')) {
            braceStart = i;
            break;
        }
    }
    if (braceStart == std::string::npos) return {};

    // find matching unescaped closing brace (not }})
    size_t braceEnd = std::string::npos;
    for (size_t i = braceStart + 1; i < command.size(); i++) {
        if (command[i] == '}' && (i + 1 >= command.size() || command[i + 1] != '}')) {
            braceEnd = i;
            break;
        }
    }
    if (braceEnd == std::string::npos) return {};

    std::string prefix = unescapeDoubleBraces(std::string_view(command.data(), braceStart));
    std::string suffix = unescapeDoubleBraces(std::string_view(command.data() + braceEnd + 1));
    std::string braceContent = unescapeDoubleBraces(std::string_view(command.data() + braceStart + 1, braceEnd - braceStart - 1));

    // split brace content by commas and trim whitespace
    std::vector<std::string> items;
    for (size_t start = 0; start < braceContent.size();) {
        size_t commaPos = braceContent.find(',', start);
        std::string item = trim(commaPos == std::string::npos ? braceContent.substr(start) : braceContent.substr(start, commaPos - start));
        if (!item.empty()) items.push_back(item);
        if (commaPos == std::string::npos) break;
        start = commaPos + 1;
    }

    if (items.empty()) return {};
    // expand by combining: prefix + item + suffix
    std::vector<std::string> result;
    result.reserve(items.size());
    for (const auto& item : items) result.push_back(prefix + item + suffix);
    return result;
}

InterpreterResult Interpreter::interpret(const ast::Program& program) {
    InterpreterResult result{};
    defines.clear();
    cache.clear();

    // first pass: collect modifier definitions and process config properties
    for (const auto& stmt : program.statements) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, ast::DefineModifierStmt>) {
                defines[node.name] = node.parts;
            } else if constexpr (std::is_same_v<T, ast::ConfigPropertyStmt>) {
                if (node.name == "blacklist") {
                    if (!node.listValues.empty()) {
                        result.config.blacklist = node.listValues;
                    } else {
                        warn("blacklist config provided without any process names; ignoring");
                    }
                    return;
                }

                if (!node.intValue) {
                    warn(
                        "config property '{}' requires an integer value. Skipping this property",
                        node.name);
                    return;
                }

                auto ms = std::chrono::milliseconds(*node.intValue);
                if (node.name == "max_chord_interval") result.config.maxChordInterval = ms;
                else if (node.name == "hold_modifier_threshold")
                    result.config.holdModifierThreshold = ms;
                else if (node.name == "simultaneous_threshold")
                    result.config.simultaneousThreshold = ms;
                else {
                    warn("unknown config property: '{}'. Valid properties are: max_chord_interval, hold_modifier_threshold, simultaneous_threshold, blacklist",
                        node.name);
                    // skip this config property
                }
            }
        },
            stmt);
    }

    // second pass: process hotkeys and handle brace expansions
    for (const auto& stmt : program.statements) {
        if (!std::holds_alternative<ast::HotkeyStmt>(stmt)) continue;
        const auto& h = std::get<ast::HotkeyStmt>(stmt);
        const auto& syn = h.syntax;
        Hotkey base = buildBaseHotkey(syn);

        // skip hotkeys with invalid modifiers (ie empty chords)
        if (base.chords.empty()) {
            continue;
        }

        // find brace expansion chord if any (only one brace expansion per hotkey)
        int braceChordIndex = -1;
        for (int i = 0; i < syn.chords.size(); i++) {
            if (syn.chords[i].key && syn.chords[i].key->isBraceExpansion) {
                braceChordIndex = i;
                break;
            }
        }

        std::vector<std::string> commandExpansions = parseCommandBraceExpansion(h.command);
        size_t expansionCount = braceChordIndex >= 0 ? syn.chords[braceChordIndex].key->items.size() : 1;

        // make sure command brace expansion matches key brace expansion count
        if (!commandExpansions.empty() && commandExpansions.size() != expansionCount) {
            warn("brace expansion mismatch: found {} key items in brace expansion but found {} command expansions. Skipping this hotkey.",
                expansionCount, commandExpansions.size());
            continue;  // Skip this hotkey
        }

        // generate hotkeys for each expansion
        for (size_t i = 0; i < expansionCount; i++) {
            Hotkey hk = base;
            setHotkeyKeys(hk, syn, braceChordIndex, i);
            // skip invalid hotkey
            if (hk.chords.empty()) {
                continue;
            }
            std::string command = commandExpansions.empty() ? h.command : commandExpansions[i];
            debug("adding command: {} : {}", hk, command);
            result.hotkeys[hk] = command;
        }
    }
    return result;
}
