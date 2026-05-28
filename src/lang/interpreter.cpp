#include "interpreter.hpp"

#include <algorithm>
#include <unordered_map>
#include <variant>

#include "../common/log.hpp"
#include "../common/string_util.hpp"
#include "lang/ast.hpp"

namespace {

class Interpreter {
   public:
    Interpreter() = default;

    InterpreterResult interpret(const ast::Program& p);
    ChordResult interpretChord(const ast::Chord& ch);

   private:
    std::unordered_map<std::string, std::vector<ast::Modifier>> defines;
    std::unordered_map<std::string, int> cache;
    std::vector<InterpreterError> errors_;

    void addError(std::string message);

    // modifier resolution
    std::optional<int> resolveModifierFlags(const std::string& name);
    std::optional<int> resolveModifiers(const std::vector<ast::Modifier>& modifiers);

    // hotkey building
    void setChordKeyFromKeysym(Chord& chord, const ast::SimpleKeysym& ks);
    std::optional<Hotkey> buildBaseHotkey(const ast::Chords& syn);
    std::optional<Chord> buildChord(const ast::Chord& chord);
    bool setHotkeyKeys(Hotkey& hk, const ast::Chords& syn, std::optional<size_t> braceChordIndex, size_t braceItemIndex);

    // command parsing
    static std::string trim(std::string_view s);
    static std::string unescapeDoubleBraces(std::string_view s);
    std::vector<std::string> parseCommandBraceExpansion(const std::string& command);

    // statement application
    void applyDefine(const ast::DefineModifier& node);
    void applyConfig(const ast::ConfigProperty& node, ConfigProperties& config);
    void applyRemap(const ast::Remap& node, std::vector<RemapBinding>& remaps);
    void applyHotkey(const ast::Hotkey& h, std::map<Hotkey, std::string>& hotkeys);
};

void Interpreter::addError(std::string message) {
    errors_.push_back(InterpreterError{.message = std::move(message)});
}

// NOLINTNEXTLINE(misc-no-recursion)
std::optional<int> Interpreter::resolveModifierFlags(const std::string& name) {
    std::vector<std::string> active;

    // NOLINTNEXTLINE(misc-no-recursion)
    const auto resolve = [&](const auto& self, const std::string& currentName) -> std::optional<int> {
        if (auto it = cache.find(currentName); it != cache.end()) return it->second;
        if (auto bi = parseBuiltinModifier(currentName)) {
            const int flags = builtinModifierToFlags(*bi);
            cache[currentName] = flags;
            return flags;
        }
        if (std::ranges::find(active, currentName) != active.end()) {
            addError(std::format("cyclic modifier reference involving '{}'", currentName));
            return std::nullopt;
        }

        auto it = defines.find(currentName);
        if (it == defines.end()) {
            addError(std::format("unknown modifier '{}'", currentName));
            return std::nullopt;
        }

        active.push_back(currentName);
        int flags = 0;
        for (const auto& part : it->second) {
            std::optional<int> partFlags =
                std::holds_alternative<BuiltinModifier>(part.value)
                    ? std::optional<int>(builtinModifierToFlags(std::get<BuiltinModifier>(part.value)))
                    : self(self, std::get<std::string>(part.value));
            if (!partFlags) {
                addError(std::format("invalid modifier reference in custom modifier definition '{}'", currentName));
                active.pop_back();
                return std::nullopt;
            }
            flags |= *partFlags;
        }
        active.pop_back();
        cache[currentName] = flags;
        return flags;
    };

    return resolve(resolve, name);
}

void Interpreter::setChordKeyFromKeysym(Chord& chord, const ast::SimpleKeysym& ks) {
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
        ks.value);
}

std::optional<int> Interpreter::resolveModifiers(const std::vector<ast::Modifier>& modifiers) {
    int flags = 0;
    for (const auto& mod : modifiers) {
        if (std::holds_alternative<BuiltinModifier>(mod.value)) {
            flags |= builtinModifierToFlags(std::get<BuiltinModifier>(mod.value));
        } else {
            auto resolvedFlags = resolveModifierFlags(std::get<std::string>(mod.value));
            if (!resolvedFlags) {
                return std::nullopt;
            }
            flags |= *resolvedFlags;
        }
    }
    return flags;
}

std::optional<Hotkey> Interpreter::buildBaseHotkey(const ast::Chords& syn) {
    Hotkey base{.passthrough = syn.passthrough, .repeat = syn.repeat, .on_release = syn.onRelease};
    base.chords.reserve(syn.sequence.size());
    for (const auto& chordSyn : syn.sequence) {
        auto flags = resolveModifiers(chordSyn.modifiers);
        if (!flags) return std::nullopt;
        base.chords.push_back(Chord{.modifiers = {.flags = *flags}});
    }
    return base;
}

std::optional<Chord> Interpreter::buildChord(const ast::Chord& ch) {
    auto flags = resolveModifiers(ch.modifiers);
    if (!flags) return std::nullopt;
    Chord chord{.modifiers = {.flags = *flags}};

    const auto* simple = ast::asSimple(*ch.key);
    if (!simple) {
        addError("remap target must contain exactly one key");
        return std::nullopt;
    }
    setChordKeyFromKeysym(chord, *simple);
    return chord;
}

bool Interpreter::setHotkeyKeys(Hotkey& hk, const ast::Chords& syn, std::optional<size_t> braceChordIndex, size_t braceItemIndex) {
    for (size_t i = 0; i < syn.sequence.size(); i++) {
        const auto& key = syn.sequence[i].key;
        if (!key.has_value()) {
            addError(std::format("chord {} in multi-chord sequence is missing a keysym", i + 1));
            return false;
        }
        const ast::SimpleKeysym* ks = std::visit([&](const auto& v) -> const ast::SimpleKeysym* {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, ast::SimpleKeysym>) {
                return &v;
            } else {
                if (v.alternatives.empty()) return nullptr;
                const size_t idx = (i == braceChordIndex && braceItemIndex < v.alternatives.size())
                                     ? braceItemIndex
                                     : 0;
                return &v.alternatives[idx];
            }
        },
            *key);
        if (!ks) {
            addError(std::format("chord {} in multi-chord sequence has empty brace expansion", i + 1));
            return false;
        }
        setChordKeyFromKeysym(hk.chords[i], *ks);
    }
    return true;
}

std::string Interpreter::trim(std::string_view s) {
    const auto* start = std::ranges::find_if_not(s, [](unsigned char c) { return std::isspace(c); });
    if (start == s.end()) return "";
    const auto* end = std::find_if_not(s.rbegin(), std::make_reverse_iterator(start), [](unsigned char c) { return std::isspace(c); }).base();
    return {start, end};
}

std::string Interpreter::unescapeDoubleBraces(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (i + 1 < s.size() && s[i] == s[i + 1] && (s[i] == '{' || s[i] == '}')) {
            result += s[i];
            i++;
        } else {
            result += s[i];
        }
    }
    return result;
}

std::vector<std::string> Interpreter::parseCommandBraceExpansion(const std::string& command) {
    size_t braceStart = std::string::npos;
    for (size_t i = 0; i < command.size(); i++) {
        if (command[i] == '{' && (i + 1 >= command.size() || command[i + 1] != '{')) {
            braceStart = i;
            break;
        }
    }
    if (braceStart == std::string::npos) return {};

    size_t braceEnd = std::string::npos;
    for (size_t i = braceStart + 1; i < command.size(); i++) {
        if (command[i] == '}' && (i + 1 >= command.size() || command[i + 1] != '}')) {
            braceEnd = i;
            break;
        }
    }
    if (braceEnd == std::string::npos) return {};

    // a second unescaped '{' in the suffix would silently be ignored, so flag it
    for (size_t i = braceEnd + 1; i < command.size(); i++) {
        if (command[i] == '{' && (i + 1 >= command.size() || command[i + 1] != '{')) {
            addError(std::format(
                "command brace expansion supports only one '{{...}}' group; found another at position {}. escape literal braces as '{{{{' / '}}}}'.",
                i));
            return {};
        }
    }

    const std::string_view commandView{command};
    std::string prefix = unescapeDoubleBraces(commandView.substr(0, braceStart));
    std::string suffix = unescapeDoubleBraces(commandView.substr(braceEnd + 1));
    std::string braceContent = unescapeDoubleBraces(commandView.substr(braceStart + 1, braceEnd - braceStart - 1));

    std::vector<std::string> items;
    for (size_t start = 0; start < braceContent.size();) {
        size_t commaPos = braceContent.find(',', start);
        std::string item = trim(commaPos == std::string::npos ? braceContent.substr(start) : braceContent.substr(start, commaPos - start));
        if (!item.empty()) items.push_back(item);
        if (commaPos == std::string::npos) break;
        start = commaPos + 1;
    }

    if (items.empty()) return {};
    std::vector<std::string> result;
    result.reserve(items.size());
    for (const auto& item : items) {
        // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
        result.push_back(prefix + item + suffix);
    }
    return result;
}

void Interpreter::applyDefine(const ast::DefineModifier& node) {
    defines[node.name] = node.parts;
}

void Interpreter::applyConfig(const ast::ConfigProperty& node, ConfigProperties& config) {
    if (node.name == "blacklist") {
        if (!node.listValues.empty()) {
            config.blacklist.clear();
            config.blacklist.reserve(node.listValues.size());
            // pre-lowercase everything
            for (const auto& value : node.listValues) {
                config.blacklist.push_back(toLower(value));
            }
        } else {
            addError("blacklist config provided without any process names; ignoring");
        }
        return;
    }

    if (!node.intValue) {
        addError(std::format(
            "config property '{}' requires an integer value. Skipping this property",
            node.name));
        return;
    }

    auto ms = std::chrono::milliseconds(*node.intValue);
    if (node.name == "max_chord_interval") config.maxChordInterval = ms;
    else if (node.name == "hold_modifier_threshold") config.holdModifierThreshold = ms;
    else if (node.name == "simultaneous_threshold") config.simultaneousThreshold = ms;
    else {
        addError(std::format(
            "unknown config property: '{}'. Valid properties are: max_chord_interval, hold_modifier_threshold, simultaneous_threshold, blacklist",
            node.name));
    }
}

void Interpreter::applyRemap(const ast::Remap& node, std::vector<RemapBinding>& remaps) {
    if (node.source.passthrough || node.source.repeat || node.source.onRelease) {
        addError("remaps do not support '@', '&', or '^' flags");
        return;
    }
    auto source = buildBaseHotkey(node.source);
    if (!source) {
        return;
    }
    if (source->chords.size() != 1) {
        addError("remaps currently support single-chord sources only");
        return;
    }
    if (node.source.sequence[0].key && ast::isBrace(*node.source.sequence[0].key)) {
        addError("remaps do not support brace expansion in the source key");
        return;
    }
    if (!setHotkeyKeys(*source, node.source, std::nullopt, 0)) {
        return;
    }
    auto target = buildChord(node.target);
    if (!target) {
        return;
    }
    if (target->modifiers.has(Hotkey_Flag_NX)) {
        addError("remaps do not support media-key targets yet");
        return;
    }
    remaps.emplace_back(std::move(*source), *target);
}

void Interpreter::applyHotkey(const ast::Hotkey& h, std::map<Hotkey, std::string>& hotkeys) {
    const auto& syn = h.chords;
    auto base = buildBaseHotkey(syn);
    if (!base) {
        return;
    }

    std::optional<size_t> braceChordIndex;
    for (size_t i = 0; i < syn.sequence.size(); i++) {
        if (syn.sequence[i].key && ast::isBrace(*syn.sequence[i].key)) {
            braceChordIndex = i;
            break;
        }
    }

    const size_t errorCountBeforeCommandExpansion = errors_.size();
    std::vector<std::string> commandExpansions = parseCommandBraceExpansion(h.command);
    // got more errors, so skip hotkey processing
    if (errors_.size() != errorCountBeforeCommandExpansion) {
        return;
    }

    size_t expansionCount = braceChordIndex
                              ? ast::asBrace(*syn.sequence[*braceChordIndex].key)->alternatives.size()
                              : 1;

    if (!commandExpansions.empty() && commandExpansions.size() != expansionCount) {
        addError(std::format(
            "brace expansion mismatch: found {} key items in brace expansion but found {} command expansions. Skipping this hotkey.",
            expansionCount,
            commandExpansions.size()));
        return;
    }

    for (size_t i = 0; i < expansionCount; i++) {
        Hotkey hk = *base;
        if (!setHotkeyKeys(hk, syn, braceChordIndex, i)) {
            continue;
        }
        std::string command = commandExpansions.empty() ? unescapeDoubleBraces(h.command) : commandExpansions[i];
        debug("adding command: {} : {}", hk, command);
        hotkeys[hk] = command;
    }
}

InterpreterResult Interpreter::interpret(const ast::Program& p) {
    InterpreterResult result{};

    // first pass: defines, config, and remaps
    for (const auto& stmt : p.statements) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, ast::DefineModifier>) {
                applyDefine(node);
            } else if constexpr (std::is_same_v<T, ast::ConfigProperty>) {
                applyConfig(node, result.config);
            } else if constexpr (std::is_same_v<T, ast::Remap>) {
                applyRemap(node, result.remaps);
            }
        },
            stmt);
    }

    // second pass: hotkeys (need all defines resolved first)
    for (const auto& stmt : p.statements) {
        if (!std::holds_alternative<ast::Hotkey>(stmt)) continue;
        applyHotkey(std::get<ast::Hotkey>(stmt), result.hotkeys);
    }
    result.errors = std::move(errors_);
    return result;
}

ChordResult Interpreter::interpretChord(const ast::Chord& ch) {
    auto chord = buildChord(ch);
    return ChordResult{.chord = chord, .errors = std::move(errors_)};
}

}  // namespace

InterpreterResult interpretProgram(const ast::Program& p) {
    return Interpreter{}.interpret(p);
}

ChordResult interpretChord(const ast::Chord& ch) {
    return Interpreter{}.interpretChord(ch);
}
