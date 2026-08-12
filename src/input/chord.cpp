#include "chord.hpp"

bool Chord::isActivatedBy(const Chord& eventInput, int liveFingerCount) const {
    if (fingerCount.has_value() && *fingerCount != liveFingerCount) {
        return false;
    }
    return modifiers.isActivatedBy(eventInput.modifiers) && this->keysym == eventInput.keysym;
}
