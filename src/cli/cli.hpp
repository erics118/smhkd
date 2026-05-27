#pragma once

#include <format>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cli {

struct Config {
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

Args parseArgs(const std::vector<std::string>& argv, const Config& config);

}  // namespace cli

template <>
struct std::formatter<cli::Args> : std::formatter<std::string_view> {
    auto format(const cli::Args& args, std::format_context& ctx) const {
        auto out = ctx.out();
        for (const auto& [key, value] : args.args) {
            out = std::format_to(out, "{}: {}\n", key, value);
        }
        return out;
    }
};
