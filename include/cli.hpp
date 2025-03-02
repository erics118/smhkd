#pragma once

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

    [[nodiscard]] bool contains(const std::string& arg) const {
        return args.contains(arg);
    }

    [[nodiscard]] bool contains(char short_arg) const {
        return contains(std::string{short_arg});
    }

    [[nodiscard]] std::optional<std::string> get(const std::string& arg) const {
        if (args.contains(arg)) {
            return args.at(arg);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> get(char short_arg) const {
        return get(std::string{short_arg});
    }

    [[nodiscard]] std::optional<std::string> get(char short_arg, const std::string& long_arg) const {
        if (auto short_result = get(short_arg)) {
            return short_result;
        }
        return get(long_arg);
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
