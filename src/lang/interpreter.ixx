module;

#include <cctype>
#include <chrono>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module smhkd.interpreter;
import smhkd.ast;
import smhkd.chord;
import smhkd.hotkey;
import smhkd.locale;
import smhkd.keysym;
import smhkd.modifier;
import smhkd.log;

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

struct DefineData {
    std::string name;
    std::vector<ast::ModifierAtom> parts;
};

class DefineResolver {
   public:
    explicit DefineResolver(std::vector<DefineData> defines) : defines_(std::move(defines)) {}
    int resolveFlags(const std::string& name) {
        if (resolving_.contains(name)) {
            throw std::runtime_error("Cyclic define_modifier detected: " + name);
        }
        if (auto it = cache_.find(name); it != cache_.end()) {
            return it->second;
        }
        resolving_.insert(name);
        if (auto bi = parseBuiltinModifier(name)) {
            int builtin = builtinModifierToFlags(*bi);
            resolving_.erase(name);
            cache_[name] = builtin;
            return builtin;
        }
        int flags = 0;
        for (const auto& d : defines_) {
            if (d.name == name) {
                for (const auto& part : d.parts) {
                    int partFlags = 0;
                    if (std::holds_alternative<BuiltinModifier>(part.value)) {
                        partFlags = builtinModifierToFlags(std::get<BuiltinModifier>(part.value));
                    } else {
                        partFlags = resolveFlags(std::get<std::string>(part.value));
                    }
                    if (partFlags == 0) {
                        resolving_.erase(name);
                        cache_[name] = 0;
                        return 0;
                    }
                    flags |= partFlags;
                }
                resolving_.erase(name);
                cache_[name] = flags;
                return flags;
            }
        }
        resolving_.erase(name);
        cache_[name] = 0;
        return 0;
    }

   private:
    std::vector<DefineData> defines_;
    std::unordered_map<std::string, int> cache_;
    std::unordered_set<std::string> resolving_;
};

void setChordKeyFromAtom(Chord& chord, const ast::KeyAtom& atom) {
    std::visit(
        [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, LiteralKey>) {
                chord.keysym.keycode = literalKeyToKeycode(v);
                chord.modifiers.flags |= getImplicitFlags(v);
            } else if constexpr (std::is_same_v<T, ast::KeyChar>) {
                if (v.isHex) {
                    chord.keysym.keycode = static_cast<uint32_t>(static_cast<unsigned char>(v.value));
                } else {
                    std::string s(1, v.value);
                    chord.keysym.keycode = getKeycode(s);
                }
            }
        },
        atom.value);
}

// Parse brace expansion from a command string, e.g., "echo {apple,banana}" -> ["echo apple", "echo banana"]
// Returns empty vector if no brace expansion found
std::vector<std::string> parseCommandBraceExpansion(const std::string& command) {
    std::vector<std::string> result;
    size_t braceStart = command.find('{');
    if (braceStart == std::string::npos) {
        return result;  // No brace expansion found
    }

    size_t braceEnd = command.find('}', braceStart);
    if (braceEnd == std::string::npos) {
        return result;  // Unmatched brace
    }

    // Extract the prefix (before the brace) and suffix (after the brace)
    std::string prefix = command.substr(0, braceStart);
    std::string suffix = command.substr(braceEnd + 1);

    // Parse the items inside the braces
    std::string braceContent = command.substr(braceStart + 1, braceEnd - braceStart - 1);
    std::vector<std::string> items;
    size_t start = 0;
    while (start < braceContent.size()) {
        // Skip leading whitespace
        while (start < braceContent.size() && std::isspace(static_cast<unsigned char>(braceContent[start]))) {
            start++;
        }
        if (start >= braceContent.size()) break;

        size_t commaPos = braceContent.find(',', start);
        if (commaPos == std::string::npos) {
            // Last item - trim trailing whitespace
            std::string item = braceContent.substr(start);
            size_t end = item.size();
            while (end > 0 && std::isspace(static_cast<unsigned char>(item[end - 1]))) {
                end--;
            }
            if (end > 0) {
                items.push_back(item.substr(0, end));
            }
            break;
        }
        // Extract item and trim trailing whitespace
        std::string item = braceContent.substr(start, commaPos - start);
        size_t end = item.size();
        while (end > 0 && std::isspace(static_cast<unsigned char>(item[end - 1]))) {
            end--;
        }
        if (end > 0) {
            items.push_back(item.substr(0, end));
        }
        start = commaPos + 1;
    }

    if (items.empty()) {
        return result;  // Empty brace expansion
    }

    // Expand each item
    for (const auto& item : items) {
        std::string expanded = prefix + item + suffix;
        result.push_back(expanded);
    }

    return result;
}

InterpreterResult Interpreter::interpret(const ast::Program& program) {
    InterpreterResult result{};

    struct ConfigProperties {
        std::vector<DefineData> defines;
        std::vector<ast::ConfigPropertyStmt> configProps;
        std::vector<ast::HotkeyStmt> hotkeyStmts;
    } acc;

    for (const auto& stmt : program.statements) {
        std::visit(
            [&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, ast::DefineModifierStmt>) {
                    acc.defines.push_back(DefineData{node.name, node.parts});
                } else if constexpr (std::is_same_v<T, ast::ConfigPropertyStmt>) {
                    acc.configProps.push_back(node);
                } else if constexpr (std::is_same_v<T, ast::HotkeyStmt>) {
                    acc.hotkeyStmts.push_back(node);
                }
            },
            stmt);
    }
    DefineResolver resolver(acc.defines);
    for (const auto& c : acc.configProps) {
        if (c.name == "max_chord_interval") {
            result.config.maxChordInterval = std::chrono::milliseconds(c.value);
        } else if (c.name == "hold_modifier_threshold") {
            result.config.holdModifierThreshold = std::chrono::milliseconds(c.value);
        } else if (c.name == "simultaneous_threshold") {
            result.config.simultaneousThreshold = std::chrono::milliseconds(c.value);
        } else {
            throw std::runtime_error("Unknown config property: " + c.name);
        }
    }
    for (const auto& h : acc.hotkeyStmts) {
        const auto& syn = h.syntax;
        Hotkey base{
            .passthrough = syn.passthrough,
            .repeat = syn.repeat,
            .on_release = syn.onRelease,
            .chords = {},
        };
        for (const auto& chordSyn : syn.chords) {
            Chord c{};
            c.modifiers.flags = 0;
            for (const auto& modName : chordSyn.modifiers) {
                int flags = 0;
                if (std::holds_alternative<BuiltinModifier>(modName.value)) {
                    flags = builtinModifierToFlags(std::get<BuiltinModifier>(modName.value));
                } else {
                    const auto& cname = std::get<std::string>(modName.value);
                    flags = resolver.resolveFlags(cname);
                }
                c.modifiers.flags |= flags;
            }
            base.chords.push_back(c);
        }
        int braceChordIndex = -1;
        if (!syn.chords.empty()) {
            for (int i = 0; i < static_cast<int>(syn.chords.size()); i++) {
                if (syn.chords[i].key && syn.chords[i].key->isBraceExpansion) {
                    braceChordIndex = i;
                    break;
                }
            }
        }
        if (braceChordIndex == -1) {
            Hotkey hk = base;
            for (size_t i = 0; i < syn.chords.size(); i++) {
                if (!syn.chords[i].key.has_value() || syn.chords[i].key->items.empty()) {
                    throw std::runtime_error("Chord missing keysym in hotkey");
                }
                setChordKeyFromAtom(hk.chords[i], syn.chords[i].key->items.front());
            }
            result.hotkeys[hk] = h.command;
            continue;
        }

        const auto& keyItems = syn.chords[braceChordIndex].key->items;
        std::vector<std::string> commandExpansions = parseCommandBraceExpansion(h.command);

        // If command has brace expansion, it must match the number of key items
        if (!commandExpansions.empty()) {
            if (commandExpansions.size() != keyItems.size()) {
                throw std::runtime_error("Brace expansion mismatch: " + std::to_string(keyItems.size()) + " key items but " + std::to_string(commandExpansions.size()) + " command expansions");
            }
        }

        for (size_t i = 0; i < keyItems.size(); i++) {
            Hotkey hk = base;
            for (size_t ci = 0; ci < syn.chords.size(); ci++) {
                if (!syn.chords[ci].key.has_value() || syn.chords[ci].key->items.empty()) {
                    throw std::runtime_error("Chord missing keysym in hotkey");
                }
                if (static_cast<int>(ci) == braceChordIndex) {
                    setChordKeyFromAtom(hk.chords[ci], keyItems[i]);
                } else {
                    setChordKeyFromAtom(hk.chords[ci], syn.chords[ci].key->items.front());
                }
            }
            // Use expanded command if available, otherwise use original
            std::string command = commandExpansions.empty() ? h.command : commandExpansions[i];
            debug("h.command: {}", command);
            result.hotkeys[hk] = command;
        }
    }
    return result;
}
