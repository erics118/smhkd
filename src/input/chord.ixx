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

    [[nodiscard]] bool isActivatedBy(const Chord& eventInput) const;
};

bool Chord::isActivatedBy(const Chord& eventInput) const {
    return modifiers.isActivatedBy(eventInput.modifiers) && this->keysym == eventInput.keysym;
}
