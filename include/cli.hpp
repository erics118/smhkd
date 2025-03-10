#pragma once

#include <format>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// format for cli args
struct ArgsConfig {
    std::unordered_set<std::string> short_args;
    std::unordered_set<std::string> long_args;
};

struct Args {
    // results
    std::unordered_map<std::string, std::string> args;

    // contains a long arg
    [[nodiscard]] bool contains(const std::string& arg) const {
        return args.contains(arg);
    }

    // contains a short arg
    [[nodiscard]] bool contains(char short_arg) const {
        return contains(std::string{short_arg});
    }

    // get the value of a long arg
    [[nodiscard]] std::optional<std::string> get(const std::string& arg) const {
        if (args.contains(arg)) {
            return args.at(arg);
        }
        return std::nullopt;
    }

    // get the value of a short arg
    [[nodiscard]] std::optional<std::string> get(char short_arg) const {
        return get(std::string{short_arg});
    }

    // get the value of a short arg, or corresponding long arg
    [[nodiscard]] std::optional<std::string> get(char short_arg, const std::string& long_arg) const {
        if (auto short_result = get(short_arg)) {
            return short_result;
        }
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

/**
 * super simple flags parser, inspired by: https://www.npmjs.com/package/simple-args-parser
 * - allows multiple short options put together (`-abc` = `-a -b -c`)
 * - silently ignores incorrect usage and unknown args
 * - doesn't support using equal signs (only `-a value`, not `-a=value`)
 * - ending with a `:` means that it takes in a value
 */
Args parse_args(const std::vector<std::string>& argv, const ArgsConfig& config);
