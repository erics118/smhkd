#include "utils.hpp"

#include <sys/stat.h>

#include <cstdlib>
#include <format>
#include <optional>
#include <string>

#include "hotkey.hpp"

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

bool has_flags(const Hotkey& hotkey, uint32_t flag) {
    bool result = hotkey.flags & flag;
    return result;
}
