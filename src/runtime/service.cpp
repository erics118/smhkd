#include "service.hpp"

#include <filesystem>
#include <fstream>

#include "../common/log.hpp"

std::string get_home_directory() {
    if (const char* homeDir = std::getenv("HOME")) {
        return homeDir;
    }
    error("failed to get home directory path");
    return "";
}

std::string get_plist_path() {
    auto home = get_home_directory();
    if (home.empty()) {
        error("failed to get plist path");
    }
    return std::format("{}/Library/LaunchAgents/{}.plist", home, PLIST_NAME);
}

std::string get_plist_contents() {
    const char* user = getenv("USER");
    if (!user) {
        error("USER environment variable not set");
    }
    const char* path_env = getenv("PATH");
    if (!path_env) {
        error("PATH environment variable not set");
    }
    std::array<char, 4096> exe_path{};
    if (proc_pidpath(getpid(), exe_path.data(), exe_path.size()) <= 0) {
        error_errno("failed to get executable path");
    }
    auto contents = std::format(PLIST_TEMPLATE, PLIST_NAME, exe_path.data(), path_env, user, user);
    if (contents.empty()) {
        error("failed to get plist contents");
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
    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    auto result = launchctl_exec({"blame", service_target}, true);
    return result == 0;
}

void service_install() {
    auto plist_path = get_plist_path();
    if (std::filesystem::exists(plist_path)) {
        error("service file '{}' is already installed", plist_path);
    }
    auto plist_contents = get_plist_contents();
    std::filesystem::create_directories(std::filesystem::path{plist_path}.parent_path());
    std::ofstream file(plist_path);
    if (!file) {
        error("failed to open '{}' for writing", plist_path);
    }
    file << plist_contents;
    if (!file) {
        error("failed to write to '{}'", plist_path);
    }
}

void service_uninstall() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        error("service file '{}' is not installed", plist_path);
    }
    std::error_code ec;
    std::filesystem::remove(plist_path, ec);
    if (ec) {
        error_error_code(std::format("failed to remove service file '{}'", plist_path), ec);
    }
}

void service_start() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        warn("service file '{}' does not exist, installing", plist_path);
        service_install();
    }
    auto service_target = get_service_target();
    auto domain_target = get_domain_target();
    if (!is_service_bootstrapped()) {
        launchctl_exec({"enable", service_target}, true);
        launchctl_exec({"bootstrap", domain_target, plist_path}, true);
    } else {
        launchctl_exec({"kickstart", service_target}, true);
    }
}

void service_restart() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        error("service file '{}' is not installed", plist_path);
    }
    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    launchctl_exec({"kickstart", "-k", service_target});
}

void service_stop() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        error("service file '{}' is not installed", plist_path);
    }
    auto service_target = get_service_target();
    auto domain_target = get_domain_target();
    if (!is_service_bootstrapped()) {
        launchctl_exec({"kill", "SIGTERM", service_target}, true);
    } else {
        launchctl_exec({"bootout", domain_target, plist_path}, true);
        launchctl_exec({"disable", service_target}, true);
    }
}
