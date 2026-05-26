#include "chord.hpp"

bool Chord::isActivatedBy(const Chord& eventInput) const {
    return modifiers.isActivatedBy(eventInput.modifiers) && this->keysym == eventInput.keysym;
}
