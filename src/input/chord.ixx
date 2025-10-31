module;

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <compare>

export module smhkd.chord;
import smhkd.token;
import smhkd.keysym;
import smhkd.modifier;
import smhkd.locale;

export struct Chord {
    Keysym keysym;
    ModifierFlags modifiers;
    std::strong_ordering operator<=>(const Chord& other) const = default;
    void setKeycode(const Token& t);
    [[nodiscard]] bool isActivatedBy(const Chord& other) const;
};

void Chord::setKeycode(const Token& t) {
    if (t.type == TokenType::Literal) {
        keysym.keycode = getKeycode(t.text);
        modifiers.flags |= getImplicitFlags(t.text);
    }
    if (t.type == TokenType::Key || t.type == TokenType::KeyHex) {
        keysym.keycode = getKeycode(t.text);
    }
}

bool Chord::isActivatedBy(const Chord& other) const {
    return modifiers.isActivatedBy(other.modifiers) && this->keysym == other.keysym;
}
