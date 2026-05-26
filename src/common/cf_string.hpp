#pragma once

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>

#include <format>
#include <string>

[[nodiscard]] std::string cfStringToString(CFStringRef cfString);

template <>
struct std::formatter<CFStringRef> : std::formatter<std::string_view> {
    auto format(const CFStringRef& cfString, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", cfStringToString(cfString));
    }
};
