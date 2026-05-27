#include "config_path.hpp"

#include <pwd.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>

#include "log.hpp"

namespace {

[[nodiscard]] bool fileExists(const std::filesystem::path& filename) {
    return std::filesystem::exists(filename) && std::filesystem::is_regular_file(filename);
}

std::optional<std::filesystem::path> getHome() {
    if (const char* home = std::getenv("HOME")) {
        return home;
    }

    if (passwd* pw = getpwuid(getuid())) {
        if (pw->pw_dir) {
            return pw->pw_dir;
        }
    }

    return std::nullopt;
}

}  // namespace

void ensureConfigFile(const std::filesystem::path& configFile) {
    if (configFile.empty()) {
        fatal("config file path is empty");
    }
    if (!fileExists(configFile)) {
        fatal("config file not found: {}", configFile.string());
    }
}

std::optional<std::filesystem::path> getConfigFile(const std::string& name) {
    std::filesystem::path path;

    char* xdgHome = getenv("XDG_CONFIG_HOME");
    if (xdgHome && *xdgHome) {
        path = std::filesystem::path{xdgHome} / name / (name + "rc");
        if (fileExists(path)) return path;
    }

    auto home = getHome();
    if (!home) return {};

    path = *home / ".config" / name / (name + "rc");
    if (fileExists(path)) return path;

    path = *home / ("." + name) / (name + "rc");
    if (fileExists(path)) return path;

    return {};
}
