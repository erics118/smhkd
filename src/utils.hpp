#pragma once

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>
#include <unistd.h>

#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "input/log.hpp"

[[nodiscard]] std::string cfStringToString(CFStringRef cfString);

template <>
struct std::formatter<CFStringRef> : std::formatter<std::string_view> {
    auto format(const CFStringRef& cfString, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", cfStringToString(cfString));
    }
};

[[nodiscard]] bool file_exists(const std::string& filename);

void validate_config_file(const std::string& config_file);

[[nodiscard]] std::optional<std::string> get_config_file(const std::string& name);

void executeCommand(const std::string& command);
