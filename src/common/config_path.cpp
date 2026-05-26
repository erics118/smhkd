#include "config_path.hpp"

#include <cstdlib>
#include <filesystem>
#include <format>

#include "log.hpp"

namespace {

[[nodiscard]] bool file_exists(const std::string& filename) {
    return std::filesystem::exists(filename) && std::filesystem::is_regular_file(filename);
}

}  // namespace

void validate_config_file(const std::string& config_file) {
    if (config_file.empty()) {
        error("config file path is empty");
    }
    if (!file_exists(config_file)) {
        error("config file not found: {}", config_file);
    }
}

std::optional<std::string> get_config_file(const std::string& name) {
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
