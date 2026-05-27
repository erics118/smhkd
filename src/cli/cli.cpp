#include "cli.hpp"

#include "../common/log.hpp"

cli::Args cli::parseArgs(const std::vector<std::string>& argv, const cli::Config& config) {
    std::unordered_map<std::string, std::string> res;
    for (size_t i = 0; i < argv.size(); ++i) {
        std::string a = argv.at(i);
        if (a.starts_with("--")) {
            a = a.substr(2);
            if (config.long_args.contains(a)) {
                res[a] = "true";
            } else if (config.long_args.contains(a + ':')) {
                if (i + 1 == argv.size()) {
                    fatal("missing value for long argument: {}", a);
                }
                res[a] = argv.at(++i);
            } else {
                fatal("unknown long argument: {}", a);
            }
        } else if (a.starts_with("-")) {
            a = a.substr(1);
            for (size_t j = 0; j < a.length() - 1; ++j) {
                char ch = a[j];
                if (config.short_args.contains(std::string{ch})) {
                    res[std::string{ch}] = "true";
                } else {
                    fatal("unknown short argument: {}", std::string{ch});
                }
            }
            char last_char = a.back();
            if (config.short_args.contains(std::string{last_char})) {
                res[std::string{last_char}] = "true";
            } else if (config.short_args.contains(std::string{last_char} + ':')) {
                if (i + 1 == argv.size()) {
                    fatal("missing value for short argument: {}", std::string{last_char});
                }
                res[std::string{last_char}] = argv.at(++i);
            } else {
                fatal("unknown short argument: {}", std::string{last_char});
            }
        } else {
            fatal("unexpected positional argument: {}", a);
        }
    }
    return cli::Args{res};
}
