#include "interpreter.hpp"

#include <algorithm>

#include "../common/log.hpp"

void Interpreter::addError(std::string message) {
    errors_.push_back(InterpreterError{.message = std::move(message)});
}

std::optional<int> Interpreter::resolveModifierFlags(const std::string& name) {
    if (auto it = cache.find(name); it != cache.end()) return it->second;
    if (auto bi = parseBuiltinModifier(name)) {
        const int flags = builtinModifierToFlags(*bi);
        cache[name] = flags;
        return flags;
    }
    if (auto it = defines.find(name); it != defines.end()) {
        int flags = 0;
        for (const auto& part : it->second) {
            std::optional<int> partFlags =
                std::holds_alternative<BuiltinModifier>(part.value)
                    ? std::optional<int>(builtinModifierToFlags(std::get<BuiltinModifier>(part.value)))
                    : resolveModifierFlags(std::get<std::string>(part.value));
            if (!partFlags) {
                addError(std::format("invalid modifier reference in custom modifier definition '{}'", name));
                return std::nullopt;
            }
            flags |= *partFlags;
        }
        cache[name] = flags;
        return flags;
    }
    addError(std::format("unknown modifier '{}'", name));
    return std::nullopt;
}

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

std::optional<Hotkey> Interpreter::buildBaseHotkey(const ast::HotkeySyntax& syn) {
    Hotkey base{.passthrough = syn.passthrough, .repeat = syn.repeat, .on_release = syn.onRelease};
    base.chords.reserve(syn.chords.size());
    for (const auto& chordSyn : syn.chords) {
        Chord c{.modifiers = {.flags = 0}};
        for (const auto& modName : chordSyn.modifiers) {
            int flags = 0;
            if (std::holds_alternative<BuiltinModifier>(modName.value)) {
                flags = builtinModifierToFlags(std::get<BuiltinModifier>(modName.value));
            } else {
                auto resolvedFlags = resolveModifierFlags(std::get<std::string>(modName.value));
                if (!resolvedFlags) {
                    return std::nullopt;
                }
                flags = *resolvedFlags;
            }
            c.modifiers.flags |= flags;
        }
        base.chords.push_back(c);
    }
    return base;
}

std::optional<Chord> Interpreter::buildChord(const ast::ChordSyntax& syntax) {
    Chord chord{.modifiers = {.flags = 0}};
    for (const auto& modName : syntax.modifiers) {
        int flags = 0;
        if (std::holds_alternative<BuiltinModifier>(modName.value)) {
            flags = builtinModifierToFlags(std::get<BuiltinModifier>(modName.value));
        } else {
            auto resolvedFlags = resolveModifierFlags(std::get<std::string>(modName.value));
            if (!resolvedFlags) {
                return std::nullopt;
            }
            flags = *resolvedFlags;
        }
        chord.modifiers.flags |= flags;
    }

    if (!syntax.key.has_value() || syntax.key->items.empty()) {
        addError("remap target is missing a keysym");
        return std::nullopt;
    }
    if (syntax.key->isBraceExpansion || syntax.key->items.size() != 1) {
        addError("remap target must contain exactly one key");
        return std::nullopt;
    }
    setChordKeyFromAtom(chord, syntax.key->items.front());
    return chord;
}

bool Interpreter::setHotkeyKeys(Hotkey& hk, const ast::HotkeySyntax& syn, int braceChordIndex, size_t braceItemIndex) {
    for (size_t i = 0; i < syn.chords.size(); i++) {
        const auto& key = syn.chords[i].key;
        if (!key.has_value() || key->items.empty()) {
            addError(std::format("chord {} in multi-chord sequence is missing a keysym", i + 1));
            return false;
        }
        const auto& keyAtom = (i == braceChordIndex && braceItemIndex < key->items.size())
                                ? key->items[braceItemIndex]
                                : key->items.front();

        setChordKeyFromAtom(hk.chords[i], keyAtom);
    }
    return true;
}

std::string Interpreter::trim(std::string_view s) {
    auto start = std::ranges::find_if_not(s, [](unsigned char c) { return std::isspace(c); });
    if (start == s.end()) return "";
    auto end = std::find_if_not(s.rbegin(), std::make_reverse_iterator(start), [](unsigned char c) { return std::isspace(c); }).base();
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

    std::string prefix = unescapeDoubleBraces(std::string_view(command.data(), braceStart));
    std::string suffix = unescapeDoubleBraces(std::string_view(command.data() + braceEnd + 1));
    std::string braceContent = unescapeDoubleBraces(std::string_view(command.data() + braceStart + 1, braceEnd - braceStart - 1));

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
    for (const auto& item : items) result.push_back(prefix + item + suffix);
    return result;
}

InterpreterResult Interpreter::interpret(const ast::Program& program) {
    InterpreterResult result{};
    defines.clear();
    cache.clear();
    errors_.clear();

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
                if (node.name == "max_chord_interval") result.config.maxChordInterval = ms;
                else if (node.name == "hold_modifier_threshold")
                    result.config.holdModifierThreshold = ms;
                else if (node.name == "simultaneous_threshold")
                    result.config.simultaneousThreshold = ms;
                else {
                    addError(std::format(
                        "unknown config property: '{}'. Valid properties are: max_chord_interval, hold_modifier_threshold, simultaneous_threshold, blacklist",
                        node.name));
                }
            } else if constexpr (std::is_same_v<T, ast::RemapStmt>) {
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
                if (node.source.chords[0].key && node.source.chords[0].key->isBraceExpansion) {
                    addError("remaps do not support brace expansion in the source key");
                    return;
                }
                if (!setHotkeyKeys(*source, node.source, -1, 0)) {
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
                result.remaps.emplace_back(std::move(*source), std::move(*target));
            }
        },
            stmt);
    }

    for (const auto& stmt : program.statements) {
        if (!std::holds_alternative<ast::HotkeyStmt>(stmt)) continue;
        const auto& h = std::get<ast::HotkeyStmt>(stmt);
        const auto& syn = h.syntax;
        auto base = buildBaseHotkey(syn);
        if (!base) {
            continue;
        }

        int braceChordIndex = -1;
        for (int i = 0; i < syn.chords.size(); i++) {
            if (syn.chords[i].key && syn.chords[i].key->isBraceExpansion) {
                braceChordIndex = i;
                break;
            }
        }

        std::vector<std::string> commandExpansions = parseCommandBraceExpansion(h.command);
        size_t expansionCount = braceChordIndex >= 0 ? syn.chords[braceChordIndex].key->items.size() : 1;

        if (!commandExpansions.empty() && commandExpansions.size() != expansionCount) {
            addError(std::format(
                "brace expansion mismatch: found {} key items in brace expansion but found {} command expansions. Skipping this hotkey.",
                expansionCount,
                commandExpansions.size()));
            continue;
        }

        for (size_t i = 0; i < expansionCount; i++) {
            Hotkey hk = *base;
            if (!setHotkeyKeys(hk, syn, braceChordIndex, i)) {
                continue;
            }
            std::string command = commandExpansions.empty() ? h.command : commandExpansions[i];
            debug("adding command: {} : {}", hk, command);
            result.hotkeys[hk] = command;
        }
    }
    result.errors = errors_;
    return result;
}

std::optional<Chord> Interpreter::interpretChordSyntax(const ast::ChordSyntax& syntax) {
    defines.clear();
    cache.clear();
    errors_.clear();
    return buildChord(syntax);
}
