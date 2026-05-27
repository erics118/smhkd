#include "cli.hpp"

#include "../common/log.hpp"

Args parse_args(const std::vector<std::string>& argv, const ArgsConfig& config) {
    std::unordered_map<std::string, std::string> res;
    for (size_t i = 0; i < argv.size(); ++i) {
        std::string a = argv.at(i);
        if (a.starts_with("--")) {
            a = a.substr(2);
            if (config.long_args.contains(a)) {
                res[a] = "true";
            } else if (i + 1 != argv.size() && config.long_args.contains(a + ':')) {
                res[a] = argv.at(++i);
            } else {
                error("unknown long argument: {}", a);
            }
        } else if (a.starts_with("-")) {
            a = a.substr(1);
            for (size_t j = 0; j < a.length() - 1; ++j) {
                char ch = a[j];
                if (config.short_args.contains(std::string{ch})) {
                    res[std::string{ch}] = "true";
                } else {
                    error("unknown short argument: {}", std::string{ch});
                }
            }
            char last_char = a.back();
            if (config.short_args.contains(std::string{last_char})) {
                res[std::string{last_char}] = "true";
            } else if (i + 1 != argv.size() && config.short_args.contains(std::string{last_char} + ':')) {
                res[std::string{last_char}] = argv.at(++i);
            } else {
                error("unknown short argument: {}", std::string{last_char});
            }
        }
    }
    return Args{res};
}
