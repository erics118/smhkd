#include "utils.hpp"

#include <CoreFoundation/CFString.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <vector>

std::string cfStringToString(CFStringRef cfString) {
    if (!cfString) return {};

    CFIndex length = CFStringGetLength(cfString);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string result(maxSize, '\0');

    if (!CFStringGetCString(cfString, result.data(), maxSize, kCFStringEncodingUTF8)) {
        return {};
    }

    result.resize(strlen(result.c_str()));
    return result;
}

bool file_exists(const std::string& filename) {
    struct stat buffer{};
    return (stat(filename.c_str(), &buffer) == 0) && S_ISREG(buffer.st_mode);
}

std::optional<std::string> get_config_file(const std::string& name) {
    char* xdgHome = getenv("XDG_CONFIG_HOME");
    std::string path{};

    // XDG_CONFIG_HOME/dir/name
    if (xdgHome && *xdgHome) {
        path = std::format("{}/{}/{}rc", xdgHome, name, name);
        if (file_exists(path)) return path;
    }

    // Check HOME environment variable
    char* home = getenv("HOME");
    if (!home || !*home) return {};

    // ~/.config/dir/name
    path = std::format("{}/.config/{}/{}rc", home, name, name);
    if (file_exists(path)) return path;

    // ~/.dir/name
    path = std::format("{}/.{}/{}rc", home, name, name);

    if (file_exists(path)) return path;

    return {};
}

void executeCommand(const std::string& command) {
    pid_t cpid = fork();

    if (cpid < 0) {
        return;
    }

    if (cpid == 0) {
        setsid();

        // Create mutable copies of the strings
        std::vector<std::string> stringStorage = {"/bin/bash", "-c", command};
        std::vector<char*> args;

        args.reserve(stringStorage.size());

        for (auto& str : stringStorage) {
            args.push_back(str.data());
        }

        args.push_back(nullptr);

        // Execute the command
        execvp(args[0], args.data());

        int status = execvp(args[0], args.data());

        _exit(status);
    }
}
