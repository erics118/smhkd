#pragma once

#include <CoreGraphics/CGEventTypes.h>
#include <IOKit/hidsystem/ev_keymap.h>

#include <print>

struct Keysym {
    uint32_t keycode;

    std::strong_ordering operator<=>(const Keysym& other) const = default;
};

std::string getNameOfKeysym(Keysym keysym);

template <>
struct std::formatter<Keysym> : std::formatter<std::string_view> {
    auto format(Keysym keysym, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", getNameOfKeysym(keysym));
    }
};
