#pragma once

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <libproc.h>
#include <spawn.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "locale.hpp"
#include "log.hpp"
#include "utils.hpp"

constexpr std::string_view LAUNCHCTL_PATH = "/bin/launchctl";
constexpr std::string_view PLIST_NAME = "com.erics118.smhkd";
constexpr std::string_view PLIST_TEMPLATE = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>{}</string>
    <key>ProgramArguments</key>
    <array>
        <string>{}</string>
    </array>
    <key>EnvironmentVariables</key>
    <dict>
        <key>PATH</key>
        <string>{}</string>
    </dict>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <dict>
        <key>SuccessfulExit</key>
        <false/>
        <key>Crashed</key>
        <true/>
    </dict>
    <key>StandardOutPath</key>
    <string>/tmp/smhkd_{}.out.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/smhkd_{}.err.log</string>
    <key>ProcessType</key>
    <string>Interactive</string>
    <key>Nice</key>
    <integer>-20</integer>
</dict>
</plist>)";

// Forward declaration of CFURLRef function
extern "C" CFURLRef CFCopyHomeDirectoryURLForUser(void* user);

class ServiceError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// RAII wrapper for posix_spawn_file_actions
class FileActions {
    posix_spawn_file_actions_t actions;

   public:
    FileActions() {
        if (posix_spawn_file_actions_init(&actions) != 0) {
            throw ServiceError("Failed to initialize spawn file actions");
        }
    }

    ~FileActions() {
        posix_spawn_file_actions_destroy(&actions);
    }

    posix_spawn_file_actions_t* get() { return &actions; }

    void redirect_output() {
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY | O_APPEND, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY | O_APPEND, 0);
    }
};

std::expected<std::string, std::string> get_home_directory() {
    CFURLRef homeurl_ref = CFCopyHomeDirectoryURLForUser(nullptr);
    if (!homeurl_ref) {
        CFRelease(homeurl_ref);
        return std::unexpected("failed to get home directory URL");
    }
    std::string home_ref = cfStringToString(CFURLCopyFileSystemPath(homeurl_ref, kCFURLPOSIXPathStyle));

    if (home_ref.empty()) return std::unexpected("failed to get home directory path");

    return home_ref;
}

std::expected<std::string, std::string> get_plist_path() {
    auto home = get_home_directory();
    if (!home) return std::unexpected(home.error());

    return std::format("{}/Library/LaunchAgents/{}.plist", *home, PLIST_NAME);
}

std::expected<std::string, std::string> get_plist_contents() {
    const char* user = getenv("USER");
    if (!user) return std::unexpected("USER environment variable not set");

    const char* path_env = getenv("PATH");
    if (!path_env) return std::unexpected("PATH environment variable not set");

    std::array<char, 4096> exe_path{};
    if (proc_pidpath(getpid(), exe_path.data(), exe_path.size()) <= 0) {
        return std::unexpected("failed to get executable path");
    }

    return std::format(PLIST_TEMPLATE,
        PLIST_NAME, exe_path.data(), path_env, user, user);
}

std::expected<void, std::string> ensure_directory_exists(const std::filesystem::path& path) {
    try {
        std::filesystem::create_directories(path.parent_path());
        return {};
    } catch (const std::filesystem::filesystem_error& e) {
        return std::unexpected(e.what());
    }
}

std::expected<int, std::string> safe_exec(const std::vector<std::string>& args, bool suppress_output = false) {
    std::vector<char*> c_args;
    c_args.reserve(args.size() + 1);
    for (const auto& arg : args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    FileActions actions;
    if (suppress_output) {
        actions.redirect_output();
    }

    pid_t pid;
    int status = posix_spawn(&pid, c_args[0], actions.get(), nullptr, c_args.data(), nullptr);
    if (status != 0) {
        return std::unexpected("failed to spawn process");
    }

    while ((waitpid(pid, &status, 0) == -1) && (errno == EINTR)) {
        usleep(1000);
    }

    if (WIFSIGNALED(status) || WIFSTOPPED(status)) {
        return std::unexpected("process terminated abnormally");
    }

    return WEXITSTATUS(status);
}

std::expected<void, std::string> service_install() {
    auto plist_path = get_plist_path();
    if (!plist_path) return std::unexpected(plist_path.error());

    if (std::filesystem::exists(*plist_path)) {
        return std::unexpected(std::format("service file '{}' already exists", *plist_path));
    }

    auto plist_contents = get_plist_contents();
    if (!plist_contents) return std::unexpected(plist_contents.error());

    auto dir_result = ensure_directory_exists(*plist_path);
    if (!dir_result) return std::unexpected(dir_result.error());

    std::ofstream file(*plist_path);
    if (!file) {
        return std::unexpected(std::format("failed to open '{}' for writing", *plist_path));
    }

    file << *plist_contents;
    if (!file) {
        return std::unexpected(std::format("failed to write to '{}'", *plist_path));
    }

    return {};
}

std::expected<void, std::string> service_uninstall() {
    auto plist_path = get_plist_path();
    if (!plist_path) return std::unexpected(plist_path.error());

    if (!std::filesystem::exists(*plist_path)) {
        return std::unexpected(std::format("service file '{}' does not exist", *plist_path));
    }

    try {
        std::filesystem::remove(*plist_path);
        return {};
    } catch (const std::filesystem::filesystem_error& e) {
        return std::unexpected(e.what());
    }
}

std::expected<void, std::string> service_start() {
    auto plist_path = get_plist_path();
    if (!plist_path) return std::unexpected(plist_path.error());

    if (!std::filesystem::exists(*plist_path)) {
        auto install_result = service_install();
        if (!install_result) return std::unexpected(install_result.error());
    }

    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    auto domain_target = std::format("gui/{}", getuid());

    // Check if service is bootstrapped
    auto result = safe_exec(std::vector<std::string>{
                                std::string(LAUNCHCTL_PATH), "print", service_target},
        true);
    if (!result) return std::unexpected(result.error());

    if (*result != 0) {
        // Service is not bootstrapped, try to enable it
        auto enable_result = safe_exec(std::vector<std::string>{
            std::string(LAUNCHCTL_PATH), "enable", service_target});
        if (!enable_result) return std::unexpected(enable_result.error());

        // Bootstrap the service
        auto bootstrap_result = safe_exec(std::vector<std::string>{
            std::string(LAUNCHCTL_PATH), "bootstrap", domain_target, *plist_path});
        if (!bootstrap_result) return std::unexpected(bootstrap_result.error());
        return {};
    } else {
        // Service is bootstrapped, kickstart it
        auto kickstart_result = safe_exec(std::vector<std::string>{
            std::string(LAUNCHCTL_PATH), "kickstart", service_target});
        if (!kickstart_result) return std::unexpected(kickstart_result.error());
        return {};
    }
}

std::expected<void, std::string> service_restart() {
    auto plist_path = get_plist_path();
    if (!plist_path) return std::unexpected(plist_path.error());

    if (!std::filesystem::exists(*plist_path)) {
        return std::unexpected(std::format("service file '{}' does not exist", *plist_path));
    }

    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    auto result = safe_exec(std::vector<std::string>{
        std::string(LAUNCHCTL_PATH), "kickstart", "-k", service_target});
    if (!result) return std::unexpected(result.error());

    return {};
}

std::expected<void, std::string> service_stop() {
    auto plist_path = get_plist_path();
    if (!plist_path) return std::unexpected(plist_path.error());

    if (!std::filesystem::exists(*plist_path)) {
        return std::unexpected(std::format("service file '{}' does not exist", *plist_path));
    }

    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    auto domain_target = std::format("gui/{}", getuid());

    // Check if service is bootstrapped
    auto result = safe_exec(std::vector<std::string>{
                                std::string(LAUNCHCTL_PATH), "print", service_target},
        true);
    if (!result) return std::unexpected(result.error());

    if (*result != 0) {
        // Service is not bootstrapped, just try to stop it
        auto stop_result = safe_exec(std::vector<std::string>{
            std::string(LAUNCHCTL_PATH), "kill", "SIGTERM", service_target});
        if (!stop_result) return std::unexpected(stop_result.error());
    } else {
        // Service is bootstrapped, bootout and disable it
        auto bootout_result = safe_exec(std::vector<std::string>{
            std::string(LAUNCHCTL_PATH), "bootout", domain_target, *plist_path});
        if (!bootout_result) return std::unexpected(bootout_result.error());

        auto disable_result = safe_exec(std::vector<std::string>{
            std::string(LAUNCHCTL_PATH), "disable", service_target});
        if (!disable_result) return std::unexpected(disable_result.error());
    }

    return {};
}
