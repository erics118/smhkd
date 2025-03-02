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

    [[nodiscard]] bool contains(const std::string& long_arg) const {
        return args.contains(long_arg);
    }

    [[nodiscard]] bool contains(char short_arg) const {
        return contains(std::string{short_arg});
    }

    [[nodiscard]] bool contains(char short_arg, const std::string& long_arg) const {
        return contains(short_arg) || contains(long_arg);
    }

    [[nodiscard]] std::string get(const std::string& long_arg) const {
        if (args.contains(long_arg)) {
            return args.at(long_arg);
        }
        return "";
    }

    [[nodiscard]] std::string get(char short_arg) const {
        return get(std::string{short_arg});
    }

    [[nodiscard]] std::string get(char short_arg, const std::string& long_arg) const {
        auto short_arg_str = get(short_arg);
        if (!short_arg_str.empty()) {
            return short_arg_str;
        }
        return get(long_arg);
    }

    [[nodiscard]] std::optional<std::string> get_opt(const std::string& long_arg) const {
        if (args.contains(long_arg)) {
            return args.at(long_arg);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> get_opt(char short_arg) const {
        return get_opt(std::string{short_arg});
    }

    [[nodiscard]] std::optional<std::string> get_opt(char short_arg, const std::string& long_arg) const {
        auto short_arg_str = get_opt(short_arg);
        if (short_arg_str.has_value()) {
            return short_arg_str;
        }
        return get_opt(long_arg);
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
