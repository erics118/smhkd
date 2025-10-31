module;

#include <sstream>
#include <string>
#include <variant>
#include <vector>

export module smhkd.ast_printer;
import smhkd.ast;
import smhkd.keysym;
import smhkd.modifier;

std::string join(const std::vector<std::string>& items, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < items.size(); i++) {
        if (i) out += sep;
        out += items[i];
    }
    return out;
}

std::string key_atom_to_string(const KeyAtom& atom) {
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, LiteralKey>) {
                return std::string("literal:") + literalKeyToString(v);
            } else if constexpr (std::is_same_v<T, KeyChar>) {
                if (v.isHex) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned char>(v.value));
                    return std::string("hex:") + buf;
                }
                return std::string("key:") + std::string(1, v.value);
            }
            return std::string{};
        },
        atom.value);
}

std::string key_syntax_to_string(const KeySyntax& ks) {
    if (ks.items.empty()) return "<none>";
    if (!ks.isBraceExpansion) {
        return key_atom_to_string(ks.items.front());
    }
    std::vector<std::string> parts;
    parts.reserve(ks.items.size());
    for (const auto& it : ks.items) parts.push_back(key_atom_to_string(it));
    return std::string("{") + join(parts, ", ") + "}";
}

std::string modifier_to_string(const ModifierAtom& m) {
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, BuiltinModifier>) {
                return builtinModifierToString(v);
            } else {
                return v;
            }
        },
        m.value);
}

export inline std::string dump_ast(const Program& program) {
    std::ostringstream oss;
    oss << "Program{\n";
    for (const auto& s : program.statements) {
        std::visit(
            [&](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, DefineModifierStmt>) {
                    std::vector<std::string> mods;
                    mods.reserve(node.parts.size());
                    for (const auto& m : node.parts) mods.push_back(modifier_to_string(m));
                    oss << "  define_modifier: " << node.name << " = " << join(mods, " + ") << "\n";
                } else if constexpr (std::is_same_v<T, ConfigPropertyStmt>) {
                    oss << "  config: " << node.name << " = " << node.value << "\n";
                } else if constexpr (std::is_same_v<T, HotkeyStmt>) {
                    const auto& syn = node.syntax;
                    oss << "  hotkey: ";
                    if (syn.passthrough) oss << "passthrough, ";
                    if (syn.repeat) oss << "repeat, ";
                    if (syn.onRelease) oss << "onRelease, ";
                    oss << "[";
                    for (size_t i = 0; i < syn.chords.size(); i++) {
                        if (i) oss << "; ";
                        const auto& cs = syn.chords[i];
                        if (!cs.modifiers.empty()) {
                            for (const auto& mod : cs.modifiers) {
                                oss << modifier_to_string(mod) << " + ";
                            }
                        }
                        if (cs.key) {
                            oss << key_syntax_to_string(*cs.key);
                        } else {
                            oss << "<missing-key>";
                        }
                    }
                    oss << "] \"" << node.command << "\"\n";
                }
            },
            s);
    }
    oss << "}\n";
    return oss.str();
}

