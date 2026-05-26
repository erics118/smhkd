#pragma once

#include "../input/log.hpp"
#include <format>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


struct ArgsConfig {
    std::unordered_set<std::string> short_args;
    std::unordered_set<std::string> long_args;
};

struct Args {
    std::unordered_map<std::string, std::string> args;
    [[nodiscard]] bool contains(const std::string& arg) const { return args.contains(arg); }
    [[nodiscard]] bool contains(char short_arg) const { return contains(std::string{short_arg}); }
    [[nodiscard]] std::optional<std::string> get(const std::string& arg) const {
        if (args.contains(arg)) return args.at(arg);
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::string> get(char short_arg) const { return get(std::string{short_arg}); }
    [[nodiscard]] std::optional<std::string> get(char short_arg, const std::string& long_arg) const {
        if (auto short_result = get(short_arg)) return short_result;
        return get(long_arg);
    }
};

template <>
struct std::formatter<Args> : std::formatter<std::string_view> {
    auto format(const Args& args, std::format_context& ctx) const {
        std::string result;
        for (const auto& [key, value] : args.args) {
            result += std::format("{}: {}\n", key, value);
        }
        return std::format_to(ctx.out(), "{}", result);
    }
};

Args parse_args(const std::vector<std::string>& argv, const ArgsConfig& config);
