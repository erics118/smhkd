module;

#include <algorithm>
#include <chrono>
#include <map>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
};

export struct InterpreterResult {
    std::map<Hotkey, std::string> hotkeys;
    ConfigProperties config;
};

export class Interpreter {
   public:
    Interpreter() = default;
    [[nodiscard]] InterpreterResult interpret(const ast::Program& program);
};

// resolves modifier name to its flag value
// handles builtin modifiers and custom modifiers
// modifiers must be defined before usage.
int resolveModifierFlags(const std::string& name,
    const std::unordered_map<std::string, std::vector<ast::ModifierAtom>>& defines,
    std::unordered_map<std::string, int>& cache) {
    if (auto it = cache.find(name); it != cache.end()) return it->second;
    if (auto bi = parseBuiltinModifier(name)) return cache[name] = builtinModifierToFlags(*bi);
    if (auto it = defines.find(name); it != defines.end()) {
        int flags = 0;
        for (const auto& part : it->second) {
            int partFlags = std::holds_alternative<BuiltinModifier>(part.value)
                              ? builtinModifierToFlags(std::get<BuiltinModifier>(part.value))
                              : resolveModifierFlags(std::get<std::string>(part.value), defines, cache);
            if (partFlags == 0) return cache[name] = 0;  // Invalid modifier reference
            flags |= partFlags;
        }
        return cache[name] = flags;
    }
    return cache[name] = 0;  // Modifier not found
}

// Sets the chord's keycode from an AST key atom (LiteralKey or KeyChar)
void setChordKeyFromAtom(Chord& chord, const ast::KeyAtom& atom) {
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
Hotkey buildBaseHotkey(const ast::HotkeySyntax& syn,
    const std::unordered_map<std::string, std::vector<ast::ModifierAtom>>& defines,
    std::unordered_map<std::string, int>& cache) {
    Hotkey base{.passthrough = syn.passthrough, .repeat = syn.repeat, .on_release = syn.onRelease};
    base.chords.reserve(syn.chords.size());
    for (const auto& chordSyn : syn.chords) {
        Chord c{.modifiers = {.flags = 0}};
        for (const auto& modName : chordSyn.modifiers) {
            c.modifiers.flags |= std::holds_alternative<BuiltinModifier>(modName.value)
                                   ? builtinModifierToFlags(std::get<BuiltinModifier>(modName.value))
                                   : resolveModifierFlags(std::get<std::string>(modName.value), defines, cache);
        }
        base.chords.push_back(c);
    }
    return base;
}

// sets the keysyms for chords in a hotkey
void setHotkeyKeys(Hotkey& hk, const ast::HotkeySyntax& syn, int braceChordIndex, size_t braceItemIndex) {
    for (size_t i = 0; i < syn.chords.size(); i++) {
        const auto& key = syn.chords[i].key;
        if (!key.has_value() || key->items.empty()) {
            throw std::runtime_error("Chord missing keysym in hotkey");
        }
        const auto& keyAtom = (i == braceChordIndex && braceItemIndex < key->items.size())
                                ? key->items[braceItemIndex]
                                : key->items.front();

        setChordKeyFromAtom(hk.chords[i], keyAtom);
    }
}

// trims leading and trailing whitespace from a string
std::string trim(std::string_view s) {
    auto start = std::ranges::find_if_not(s, [](unsigned char c) { return std::isspace(c); });
    if (start == s.end()) return "";
    auto end = std::find_if_not(s.rbegin(), std::make_reverse_iterator(start), [](unsigned char c) { return std::isspace(c); }).base();
    return std::string(start, end);
}

// converts double braces to single braces: {{ -> { and }} -> }
// used to escape literal braces in command strings
std::string unescapeDoubleBraces(std::string_view s) {
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

// parses brace expension in command strings.
// returns a vector of the expanded commands
std::vector<std::string> parseCommandBraceExpansion(const std::string& command) {
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
    std::unordered_map<std::string, std::vector<ast::ModifierAtom>> defines;

    // first pass: collect modifier definitions and process config properties
    for (const auto& stmt : program.statements) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, ast::DefineModifierStmt>) {
                defines[node.name] = node.parts;
            } else if constexpr (std::is_same_v<T, ast::ConfigPropertyStmt>) {
                auto ms = std::chrono::milliseconds(node.value);
                if (node.name == "max_chord_interval") result.config.maxChordInterval = ms;
                else if (node.name == "hold_modifier_threshold") result.config.holdModifierThreshold = ms;
                else if (node.name == "simultaneous_threshold") result.config.simultaneousThreshold = ms;
                else throw std::runtime_error("Unknown config property: " + node.name);
            }
        },
            stmt);
    }

    std::unordered_map<std::string, int> cache;

    // second pass: process hotkeys and handle brace expansions
    for (const auto& stmt : program.statements) {
        if (!std::holds_alternative<ast::HotkeyStmt>(stmt)) continue;
        const auto& h = std::get<ast::HotkeyStmt>(stmt);
        const auto& syn = h.syntax;
        Hotkey base = buildBaseHotkey(syn, defines, cache);

        // find brace expansion chord if any (only one brace expansion per hotkey)
        int braceChordIndex = -1;
        for (int i = 0; i < static_cast<int>(syn.chords.size()); i++) {
            if (syn.chords[i].key && syn.chords[i].key->isBraceExpansion) {
                braceChordIndex = i;
                break;
            }
        }

        std::vector<std::string> commandExpansions = parseCommandBraceExpansion(h.command);
        size_t expansionCount = braceChordIndex >= 0 ? syn.chords[braceChordIndex].key->items.size() : 1;

        // make sure command brace expansion matches key brace expansion count
        if (!commandExpansions.empty() && commandExpansions.size() != expansionCount) {
            throw std::runtime_error("Brace expansion mismatch: " + std::to_string(expansionCount) + " key items but " + std::to_string(commandExpansions.size()) + " command expansions");
        }

        // generate hotkeys for each expansion
        for (size_t i = 0; i < expansionCount; i++) {
            Hotkey hk = base;
            setHotkeyKeys(hk, syn, braceChordIndex, i);
            std::string command = commandExpansions.empty() ? h.command : commandExpansions[i];
            debug("adding command: {} : {}", hk, command);
            result.hotkeys[hk] = command;
        }
    }
    return result;
}
