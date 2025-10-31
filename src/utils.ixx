module;

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <vector>

import smhkd.log;

export module smhkd.utils;

export template <>
struct std::formatter<CFStringRef> : std::formatter<std::string_view> {
    auto format(const CFStringRef& cfString, std::format_context& ctx) const {
        if (!cfString) return std::format_to(ctx.out(), "");

        CFIndex length = CFStringGetLength(cfString);
        CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        std::string result(maxSize, '\0');

        if (!CFStringGetCString(cfString, result.data(), maxSize, kCFStringEncodingUTF8)) {
            if (!cfString) return std::format_to(ctx.out(), "");
        }

        result.resize(strlen(result.c_str()));

        return std::format_to(ctx.out(), "{}", result);
    }
};

export [[nodiscard]] bool file_exists(const std::string& filename) {
    return std::filesystem::exists(filename) && std::filesystem::is_regular_file(filename);
}

export [[nodiscard]] std::optional<std::string> get_config_file(const std::string& name) {
    char* xdgHome = getenv("XDG_CONFIG_HOME");
    std::string path;

    if (xdgHome && *xdgHome) {
        path = std::format("{}/{}/{}rc", xdgHome, name, name);
        if (file_exists(path)) return path;
    }

    char* home = getenv("HOME");
    if (!home || !*home) return {};

    path = std::format("{}/.config/{}/{}rc", home, name, name);
    if (file_exists(path)) return path;

    path = std::format("{}/.{}/{}rc", home, name, name);
    if (file_exists(path)) return path;

    return {};
}

export void executeCommand(const std::string& command) {
    pid_t cpid = fork();

    if (cpid < 0) {
        return;
    }

    if (cpid == 0) {
        setsid();

        std::vector<std::string> stringStorage = {"/bin/bash", "-c", command};
        std::vector<char*> args;
        args.reserve(stringStorage.size());
        for (auto& str : stringStorage) {
            args.push_back(str.data());
        }
        args.push_back(nullptr);

        int status = execvp(args[0], args.data());
        _exit(status);
    }
}
