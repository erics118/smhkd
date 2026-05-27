#include "service.hpp"

#include <filesystem>
#include <fstream>

#include "../common/log.hpp"

namespace {

void require_launchctl_success(const std::vector<std::string>& args, std::string_view action, bool suppress_output = false) {
    const int status = launchctl_exec(args, suppress_output);
    if (status != 0) {
        fatal("launchctl {} failed with exit code {}", action, status);
    }
}

}  // namespace

std::string get_home_directory() {
    if (const char* homeDir = std::getenv("HOME")) {
        return homeDir;
    }
    fatal("failed to get home directory path");
}

std::string get_plist_path() {
    auto home = get_home_directory();
    if (home.empty()) {
        fatal("failed to get plist path");
    }
    return std::format("{}/Library/LaunchAgents/{}.plist", home, PLIST_NAME);
}

std::string get_plist_contents() {
    const char* user = getenv("USER");
    if (!user) {
        fatal("USER environment variable not set");
    }
    const char* path_env = getenv("PATH");
    if (!path_env) {
        fatal("PATH environment variable not set");
    }
    std::array<char, 4096> exe_path{};
    if (proc_pidpath(getpid(), exe_path.data(), exe_path.size()) <= 0) {
        fatal("failed to get executable path");
    }
    auto contents = std::format(PLIST_TEMPLATE, PLIST_NAME, exe_path.data(), path_env, user, user);
    if (contents.empty()) {
        fatal("failed to get plist contents");
    }
    return contents;
}

int launchctl_exec(const std::vector<std::string>& args, bool suppress_output) {
    std::vector<std::string> stringStorage;
    stringStorage.reserve(args.size() + 1);
    std::vector<char*> c_args;
    c_args.reserve(args.size() + 2);
    stringStorage.emplace_back(LAUNCHCTL_PATH);
    c_args.push_back(stringStorage.back().data());
    for (const auto& arg : args) {
        stringStorage.push_back(arg);
        c_args.push_back(stringStorage.back().data());
    }
    c_args.push_back(nullptr);
    posix_spawn_file_actions_t actions{};
    posix_spawn_file_actions_init(&actions);
    if (suppress_output) {
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY | O_APPEND, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY | O_APPEND, 0);
    }
    pid_t pid{};
    int status = posix_spawn(&pid, c_args.at(0), &actions, nullptr, c_args.data(), nullptr);
    posix_spawn_file_actions_destroy(&actions);
    if (status != 0) {
        return 1;
    }
    while ((waitpid(pid, &status, 0) == -1) && (errno == EINTR)) {
        usleep(1000);
    }
    if (WIFSIGNALED(status) || WIFSTOPPED(status)) {
        return 1;
    }
    return WEXITSTATUS(status);
}

std::string get_domain_target() {
    static const std::string target = std::format("gui/{}", getuid());
    return target;
}

std::string get_service_target() {
    static const std::string target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    return target;
}

bool is_service_bootstrapped() {
    auto result = launchctl_exec({"blame", get_service_target()}, true);
    return result == 0;
}

void service_install() {
    auto plist_path = get_plist_path();
    if (std::filesystem::exists(plist_path)) {
        fatal("service file '{}' is already installed", plist_path);
    }
    auto plist_contents = get_plist_contents();
    std::filesystem::create_directories(std::filesystem::path{plist_path}.parent_path());
    std::ofstream file(plist_path);
    if (!file) {
        fatal("failed to open '{}' for writing", plist_path);
    }
    file << plist_contents;
    if (!file) {
        fatal("failed to write to '{}'", plist_path);
    }
    info("service installed");
}

void service_uninstall() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        fatal("service file '{}' is not installed", plist_path);
    }
    std::error_code ec;
    std::filesystem::remove(plist_path, ec);
    if (ec) {
        fatal("failed to remove service file '{}': {}", plist_path, ec.message());
    }
    info("service uninstalled");
}

void service_start() {
    auto plist_path = get_plist_path();
    bool installed_during_start = false;
    if (!std::filesystem::exists(plist_path)) {
        warn("service file '{}' does not exist, installing", plist_path);
        service_install();
        installed_during_start = true;
    }
    auto service_target = get_service_target();
    auto domain_target = get_domain_target();
    const bool wasBootstrapped = is_service_bootstrapped();
    if (wasBootstrapped) {
        info("service already started");
        return;
    }

    require_launchctl_success({"enable", service_target}, "enable", true);
    require_launchctl_success({"bootstrap", domain_target, plist_path}, "bootstrap", true);

    if (!is_service_bootstrapped()) {
        fatal("service did not appear in launchctl after start");
    }

    if (installed_during_start) {
        info("service installed and started");
    } else {
        info("service started");
    }
}

void service_restart() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        fatal("service file '{}' is not installed", plist_path);
    }
    require_launchctl_success({"kickstart", "-k", get_service_target()}, "restart");
    info("service restarted");
}

void service_stop() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        fatal("service file '{}' is not installed", plist_path);
    }
    auto service_target = get_service_target();
    auto domain_target = get_domain_target();
    if (!is_service_bootstrapped()) {
        warn("service is not loaded");
        return;
    }

    require_launchctl_success({"bootout", domain_target, plist_path}, "bootout", true);
    require_launchctl_success({"disable", service_target}, "disable", true);

    info("service stopped");
}
