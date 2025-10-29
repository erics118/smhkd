#include "interpreter.hpp"


#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "chord.hpp"
#include "keysym.hpp"
#include "log.hpp"
#include "modifier.hpp"

namespace {

// Expand a command containing a single-level brace list like echo {1,2,3}
std::vector<std::string> expandCommandString(const std::string& command) {
    std::vector<std::string> result;

    size_t start = command.find('{');
    size_t end = command.find('}');

    if (start == std::string::npos || end == std::string::npos || start > end) {
        result.push_back(command);
        return result;
    }

    std::string prefix = command.substr(0, start);
    std::string content = command.substr(start + 1, end - start - 1);
    std::string suffix = command.substr(end + 1);

    size_t pos = 0;
    while (pos < content.size()) {
        size_t comma = content.find(',', pos);
        if (comma == std::string::npos) {
            comma = content.size();
        }
        std::string rest = content.substr(pos, comma - pos) + suffix;
        result.push_back(prefix + rest);
        pos = comma + 1;
    }

    return result;
}

}  // namespace

struct DefineData {
    std::string name;
    std::vector<ModifierAtom> parts;
};

class DefineResolver {
   public:
    explicit DefineResolver(std::vector<DefineData> defines) : defines_(std::move(defines)) {}

    int resolveFlags(const std::string& name) {
        if (resolving_.count(name)) {
            throw std::runtime_error("Cyclic define_modifier detected: " + name);
        }
        if (auto it = cache_.find(name); it != cache_.end()) {
            return it->second;
        }

        resolving_.insert(name);
        int flags = 0;
        // first try builtin
        if (auto bi = parseBuiltinModifier(name)) {
            int builtin = builtinModifierToFlags(*bi);
            resolving_.erase(name);
            cache_[name] = builtin;
            return builtin;
        }

        // otherwise look up custom
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

        // unknown
        resolving_.erase(name);
        cache_[name] = 0;
        return 0;
    }

   private:
    std::vector<DefineData> defines_;
    std::unordered_map<std::string, int> cache_;
    std::unordered_set<std::string> resolving_;
};

static void setChordKeyFromAtom(Chord& chord, const KeyAtom& atom) {
    std::visit(
        [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, LiteralKey>) {
                chord.keysym.keycode = literalKeyToKeycode(v);
                chord.modifiers.flags |= getImplicitFlags(literalKeyToString(v));
            } else if constexpr (std::is_same_v<T, KeyChar>) {
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

InterpreterResult Interpreter::interpret(const Program& program) {
    InterpreterResult result{};

    // Collect defines for later resolution
    std::vector<DefineData> defines;
    std::vector<ConfigPropertyStmt> configProps;
    std::vector<HotkeyStmt> hotkeyStmts;

    for (const auto& stmt : program.statements) {
        std::visit(
            [&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, DefineModifierStmt>) {
                    defines.push_back(DefineData{node.name, node.parts});
                } else if constexpr (std::is_same_v<T, ConfigPropertyStmt>) {
                    configProps.push_back(node);
                } else if constexpr (std::is_same_v<T, HotkeyStmt>) {
                    hotkeyStmts.push_back(node);
                }
            },
            stmt);
    }

    DefineResolver resolver(defines);

    // Apply config properties
    for (const auto& c : configProps) {
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

    // Build hotkeys
    for (const auto& h : hotkeyStmts) {
        const auto& syn = h.syntax;

        // Base hotkey template
        Hotkey base;
        base.passthrough = syn.passthrough;
        base.repeat = syn.repeat;
        base.on_release = syn.onRelease;
        base.chords.clear();

        // Pre-build chords with modifiers only; keys are added per-expansion
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
                    if (flags == 0) {
                        warn("Unknown modifier '{}' in hotkey", cname);
                    }
                }
                c.modifiers.flags |= flags;
            }
            base.chords.push_back(c);
        }

        // We only support a single keysym per chord in the current syntax.
        // Find if any chord uses a brace expansion; if none, emit single mapping
        int braceChordIndex = -1;
        if (!syn.chords.empty()) {
            for (int i = 0; i < static_cast<int>(syn.chords.size()); i++) {
                if (syn.chords[i].key && syn.chords[i].key->isBraceExpansion) {
                    braceChordIndex = i;
                    break;
                }
            }
        }

        if (braceChordIndex < 0) {
            // No brace expansion; set keys and emit
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

        // With brace expansion: expand keys and possibly commands
        const auto& keyItems = syn.chords[braceChordIndex].key->items;
        std::vector<std::string> commands = expandCommandString(h.command);
        if (!commands.empty() && commands.size() != keyItems.size()) {
            throw std::runtime_error("Expansion keysyms and commands must be the same size");
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
            const std::string& cmd = commands.empty() ? h.command : commands[i];
            result.hotkeys[hk] = cmd;
        }
    }

    return result;
}
