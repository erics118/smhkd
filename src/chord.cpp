#include "chord.hpp"

void Chord::setKeycode(const Token& t) {
    if (t.type == TokenType::Literal) {
        // handle implicit flags only for literals
        modifiers.flags |= getImplicitFlags(t.text);
    }
    if (t.type == TokenType::Key || t.type == TokenType::KeyHex) {
        keysym.keycodes.insert(getKeycode(t.text));
    }
}

bool Chord::isActivatedBy(const Chord& other) const {
    return modifiers.isActivatedBy(other.modifiers) && this->keysym == other.keysym;
}

bool Chord::isSimultaneous() const {
    return keysym.keycodes.size() > 1;
}
