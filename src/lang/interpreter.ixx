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

int resolveModifierFlags(const std::string& name,
    const std::unordered_map<std::string, std::vector<ast::ModifierAtom>>& defines,
    std::unordered_map<std::string, int>& cache,
    std::unordered_set<std::string>& resolving) {
    if (resolving.contains(name)) throw std::runtime_error("Cyclic define_modifier detected: " + name);
    if (auto it = cache.find(name); it != cache.end()) return it->second;

    if (auto bi = parseBuiltinModifier(name)) {
        return cache[name] = builtinModifierToFlags(*bi);
    }

    if (auto it = defines.find(name); it != defines.end()) {
        resolving.insert(name);
        int flags = 0;
        for (const auto& part : it->second) {
            int partFlags = 0;
            if (std::holds_alternative<BuiltinModifier>(part.value)) {
                partFlags = builtinModifierToFlags(std::get<BuiltinModifier>(part.value));
            } else {
                partFlags = resolveModifierFlags(std::get<std::string>(part.value), defines, cache, resolving);
            }
            if (partFlags == 0) {
                resolving.erase(name);
                return cache[name] = 0;
            }
            flags |= partFlags;
        }
        resolving.erase(name);
        return cache[name] = flags;
    }

    return cache[name] = 0;
}

void setChordKeyFromAtom(Chord& chord, const ast::KeyAtom& atom) {
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, LiteralKey>) {
            chord.keysym.keycode = literalKeyToKeycode(v);
            chord.modifiers.flags |= getImplicitFlags(v);
        } else if constexpr (std::is_same_v<T, ast::KeyChar>) {
            chord.keysym.keycode = v.isHex ? static_cast<uint32_t>(static_cast<unsigned char>(v.value))
                                           : getKeycode(std::string(1, v.value));
        }
    },
        atom.value);
}

Hotkey buildBaseHotkey(const ast::HotkeySyntax& syn,
    const std::unordered_map<std::string, std::vector<ast::ModifierAtom>>& defines,
    std::unordered_map<std::string, int>& cache,
    std::unordered_set<std::string>& resolving) {
    Hotkey base{
        .passthrough = syn.passthrough,
        .repeat = syn.repeat,
        .on_release = syn.onRelease,
    };
    base.chords.reserve(syn.chords.size());
    for (const auto& chordSyn : syn.chords) {
        Chord c{.modifiers = {.flags = 0}};
        for (const auto& modName : chordSyn.modifiers) {
            int flags = 0;
            if (std::holds_alternative<BuiltinModifier>(modName.value)) {
                flags = builtinModifierToFlags(std::get<BuiltinModifier>(modName.value));
            } else {
                flags = resolveModifierFlags(std::get<std::string>(modName.value), defines, cache, resolving);
            }
            c.modifiers.flags |= flags;
        }
        base.chords.push_back(c);
    }
    return base;
}

void setHotkeyKeys(Hotkey& hk, const ast::HotkeySyntax& syn, int braceChordIndex, size_t braceItemIndex) {
    for (size_t i = 0; i < syn.chords.size(); i++) {
        const auto& key = syn.chords[i].key;
        if (!key.has_value() || key->items.empty()) {
            throw std::runtime_error("Chord missing keysym in hotkey");
        }
        if (static_cast<int>(i) == braceChordIndex && braceItemIndex < key->items.size()) {
            setChordKeyFromAtom(hk.chords[i], key->items[braceItemIndex]);
        } else {
            setChordKeyFromAtom(hk.chords[i], key->items.front());
        }
    }
}

std::string trim(std::string_view s) {
    auto isSpace = [](unsigned char c) { return std::isspace(c); };
    auto start = std::ranges::find_if_not(s, isSpace);
    if (start == s.end()) return "";
    auto end = std::find_if_not(s.rbegin(), std::make_reverse_iterator(start), isSpace).base();
    return std::string(start, end);
}

std::vector<std::string> parseCommandBraceExpansion(const std::string& command) {
    size_t braceStart = command.find('{');
    if (braceStart == std::string::npos) return {};
    size_t braceEnd = command.find('}', braceStart);
    if (braceEnd == std::string::npos) return {};

    std::string prefix(command, 0, braceStart);
    std::string suffix(command, braceEnd + 1);
    std::string_view braceContent(command.data() + braceStart + 1, braceEnd - braceStart - 1);

    std::vector<std::string> items;
    for (size_t start = 0; start < braceContent.size();) {
        size_t commaPos = braceContent.find(',', start);
        std::string item = trim(commaPos == std::string::npos
                                    ? braceContent.substr(start)
                                    : braceContent.substr(start, commaPos - start));
        if (!item.empty()) items.push_back(item);
        if (commaPos == std::string::npos) break;
        start = commaPos + 1;
    }

    if (items.empty()) return {};
    std::vector<std::string> result;
    result.reserve(items.size());
    std::ranges::transform(items, std::back_inserter(result), [&](const std::string& item) {
        return prefix + item + suffix;
    });
    return result;
}

InterpreterResult Interpreter::interpret(const ast::Program& program) {
    InterpreterResult result{};
    std::unordered_map<std::string, std::vector<ast::ModifierAtom>> defines;

    for (const auto& stmt : program.statements) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, ast::DefineModifierStmt>) {
                defines[node.name] = node.parts;
            } else if constexpr (std::is_same_v<T, ast::ConfigPropertyStmt>) {
                auto setConfig = [&](auto& prop) { prop = std::chrono::milliseconds(node.value); };
                if (node.name == "max_chord_interval") setConfig(result.config.maxChordInterval);
                else if (node.name == "hold_modifier_threshold") setConfig(result.config.holdModifierThreshold);
                else if (node.name == "simultaneous_threshold") setConfig(result.config.simultaneousThreshold);
                else throw std::runtime_error("Unknown config property: " + node.name);
            }
        },
            stmt);
    }

    std::unordered_map<std::string, int> cache;
    std::unordered_set<std::string> resolving;

    for (const auto& stmt : program.statements) {
        if (!std::holds_alternative<ast::HotkeyStmt>(stmt)) continue;
        const auto& h = std::get<ast::HotkeyStmt>(stmt);
        const auto& syn = h.syntax;
        Hotkey base = buildBaseHotkey(syn, defines, cache, resolving);

        auto braceIt = std::ranges::find_if(syn.chords, [](const auto& c) {
            return c.key && c.key->isBraceExpansion;
        });
        int braceChordIndex = braceIt != syn.chords.end()
                                ? static_cast<int>(std::distance(syn.chords.begin(), braceIt))
                                : -1;

        std::vector<std::string> commandExpansions = parseCommandBraceExpansion(h.command);
        size_t expansionCount = 1;
        if (braceChordIndex >= 0) {
            expansionCount = syn.chords[braceChordIndex].key->items.size();
        }

        if (!commandExpansions.empty() && commandExpansions.size() != expansionCount) {
            throw std::runtime_error("Brace expansion mismatch: " + std::to_string(expansionCount) + " key items but " + std::to_string(commandExpansions.size()) + " command expansions");
        }

        for (size_t i = 0; i < expansionCount; i++) {
            Hotkey hk = base;
            setHotkeyKeys(hk, syn, braceChordIndex, i);
            std::string command = h.command;
            if (!commandExpansions.empty()) {
                command = commandExpansions[i];
            }
            debug("h.command: {}", command);
            result.hotkeys[hk] = command;
        }
    }
    return result;
}
