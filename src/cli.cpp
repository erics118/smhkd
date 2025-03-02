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
            }
        } else if (a.starts_with("-")) {  // short arg (one dash)
            a = a.substr(1);
            // in the case of -abc, loop over each char individually
            for (char ch : a) {
                if (config.short_args.contains(std::string{ch})) {
                    res[std::string{ch}] = "true";
                }
            }
            // check if the last short arg requires a value
            if (i + 1 != argv.size() && config.short_args.contains(std::string{a.back()} + ':')) {
                res[std::string{a.back()}] = argv[++i];
            }
        }
    }
    return Args{res};
}
