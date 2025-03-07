#include "chord.hpp"

void Chord::setKeycode(const Token& t) {
    if (t.type == TokenType::Literal) {
        keysym.keycode = getKeycode(t.text);
        // handle implicit flags only for literals
        modifiers.flags |= getImplicitFlags(t.text);
    }
    if (t.type == TokenType::Key || t.type == TokenType::KeyHex) {
        keysym.keycode = getKeycode(t.text);
    }
}

bool Chord::isActivatedBy(const Chord& other) const {
    return modifiers.isActivatedBy(other.modifiers) && this->keysym == other.keysym;
}
