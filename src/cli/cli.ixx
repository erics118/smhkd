module;

#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

export module smhkd.cli;

export struct ArgsConfig {
    std::unordered_set<std::string> short_args;
    std::unordered_set<std::string> long_args;
};

export struct Args {
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

export template <>
struct std::formatter<Args> : std::formatter<std::string_view> {
    auto format(const Args& args, std::format_context& ctx) const {
        std::string result;
        for (const auto& [key, value] : args.args) {
            result += std::format("{}: {}\n", key, value);
        }
        return std::format_to(ctx.out(), "{}", result);
    }
};

export inline Args parse_args(const std::vector<std::string>& argv, const ArgsConfig& config) {
    std::unordered_map<std::string, std::string> res;
    for (size_t i = 0; i < argv.size(); ++i) {
        std::string a = argv[i];
        if (a.starts_with("--")) {
            a = a.substr(2);
            if (config.long_args.contains(a)) {
                res[a] = "true";
            } else if (i + 1 != argv.size() && config.long_args.contains(a + ':')) {
                res[a] = argv[++i];
            } else {
                throw std::runtime_error("unknown long argument: " + a);
            }
        } else if (a.starts_with("-")) {
            a = a.substr(1);
            for (size_t j = 0; j < a.length() - 1; ++j) {
                char ch = a[j];
                if (config.short_args.contains(std::string{ch})) {
                    res[std::string{ch}] = "true";
                } else {
                    throw std::runtime_error("unknown short argument: " + std::string{ch});
                }
            }
            char last_char = a.back();
            if (config.short_args.contains(std::string{last_char})) {
                res[std::string{last_char}] = "true";
            } else if (i + 1 != argv.size() && config.short_args.contains(std::string{last_char} + ':')) {
                res[std::string{last_char}] = argv[++i];
            } else {
                throw std::runtime_error("unknown short argument: " + std::string{last_char});
            }
        }
    }
    return Args{res};
}
