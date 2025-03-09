#include "cli.hpp"

#include <unordered_map>
#include <vector>

Args parse_args(const std::vector<std::string>& argv, const ArgsConfig& config) {
    std::unordered_map<std::string, std::string> res;
    for (size_t i = 0; i < argv.size(); ++i) {
        std::string a = argv[i];
        if (a.starts_with("--")) {  // long arg (two dashes)
            a = a.substr(2);
            // if it's a valid long arg
            if (config.long_args.contains(a)) {
                // using it as a string is ok bc i know that it must be of type bool
                res[a] = "true";
            } else if (i + 1 != argv.size() && config.long_args.contains(a + ':')) {
                res[a] = argv[++i];
            } else {
                throw std::runtime_error("unknown long argument: " + a);
            }
        } else if (a.starts_with("-")) {  // short arg (one dash)
            a = a.substr(1);
            // in the case of -abc, loop over chars, except the last one
            for (size_t i = 0; i < a.length() - 1; ++i) {
                char ch = a[i];
                if (config.short_args.contains(std::string{ch})) {
                    res[std::string{ch}] = "true";
                } else {
                    throw std::runtime_error("unknown short argument: " + std::string{ch});
                }
            }
            // handle the last char
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
