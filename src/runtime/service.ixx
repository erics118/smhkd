module;

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <libproc.h>
#include <spawn.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

export module smhkd.service;
import smhkd.utils;

export inline constexpr std::string LAUNCHCTL_PATH = "/bin/launchctl";
export inline constexpr std::string_view PLIST_NAME = "com.erics118.smhkd";
export inline constexpr std::string_view PLIST_TEMPLATE = R"(<?xml version="1.0" encoding="UTF-8"?>
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

extern "C" CFURLRef CFCopyHomeDirectoryURLForUser(void* user);

export std::string get_home_directory();
export std::string get_plist_path();
export std::string get_plist_contents();
export int launchctl_exec(const std::vector<std::string>& args, bool suppress_output = false);
export [[nodiscard]] std::string get_service_target();
export [[nodiscard]] std::string get_domain_target();
export [[nodiscard]] bool is_service_bootstrapped();
export void service_install();
export void service_uninstall();
export void service_start();
export void service_restart();
export void service_stop();

export std::string get_home_directory() {
    CFURLRef homeurl_ref = CFCopyHomeDirectoryURLForUser(nullptr);
    if (!homeurl_ref) {
        throw std::runtime_error("failed to get home directory URL");
    }
    std::string home_ref = cfStringToString(CFURLCopyFileSystemPath(homeurl_ref, kCFURLPOSIXPathStyle));
    CFRelease(homeurl_ref);
    if (home_ref.empty()) throw std::runtime_error("failed to get home directory path");
    return home_ref;
}

export std::string get_plist_path() {
    auto home = get_home_directory();
    if (home.empty()) throw std::runtime_error("failed to get plist path");
    return std::format("{}/Library/LaunchAgents/{}.plist", home, PLIST_NAME);
}

export std::string get_plist_contents() {
    const char* user = getenv("USER");
    if (!user) throw std::runtime_error("USER environment variable not set");
    const char* path_env = getenv("PATH");
    if (!path_env) throw std::runtime_error("PATH environment variable not set");
    std::array<char, 4096> exe_path{};
    if (proc_pidpath(getpid(), exe_path.data(), exe_path.size()) <= 0) {
        throw std::runtime_error("failed to get executable path");
    }
    auto contents = std::format(PLIST_TEMPLATE, PLIST_NAME, exe_path.data(), path_env, user, user);
    if (contents.empty()) throw std::runtime_error("failed to get plist contents");
    return contents;
}

export int launchctl_exec(const std::vector<std::string>& args, bool suppress_output) {
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
    int status = posix_spawn(&pid, c_args[0], &actions, nullptr, c_args.data(), nullptr);
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

export [[nodiscard]] std::string get_domain_target() {
    static std::string target = std::format("gui/{}", getuid());
    return target;
}
export [[nodiscard]] std::string get_service_target() {
    static std::string target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    return target;
}
export [[nodiscard]] bool is_service_bootstrapped() {
    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    auto result = launchctl_exec({"blame", service_target}, true);
    return result == 0;
}

export void service_install() {
    auto plist_path = get_plist_path();
    if (std::filesystem::exists(plist_path)) {
        throw std::runtime_error(std::format("service file '{}' is already installed", plist_path));
    }
    auto plist_contents = get_plist_contents();
    std::filesystem::create_directories(std::filesystem::path{plist_path}.parent_path());
    std::ofstream file(plist_path);
    if (!file) {
        throw std::runtime_error(std::format("failed to open '{}' for writing", plist_path));
    }
    file << plist_contents;
    if (!file) {
        throw std::runtime_error(std::format("failed to write to '{}'", plist_path));
    }
}

export void service_uninstall() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        throw std::runtime_error(std::format("service file '{}' is not installed", plist_path));
    }
    std::filesystem::remove(plist_path);
}

export void service_start() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        std::print(stdout, "{}\n", std::format("service file '{}' does not exist, installing", plist_path));
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

export void service_restart() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        throw std::runtime_error(std::format("service file '{}' is not installed", plist_path));
    }
    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    launchctl_exec({"kickstart", "-k", service_target});
}

export void service_stop() {
    auto plist_path = get_plist_path();
    if (!std::filesystem::exists(plist_path)) {
        throw std::runtime_error(std::format("service file '{}' is not installed", plist_path));
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
