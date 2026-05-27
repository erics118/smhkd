#include "service.hpp"

#include <libproc.h>
#include <spawn.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "../common/log.hpp"

constexpr std::string LAUNCHCTL_PATH = "/bin/launchctl";
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

namespace {

std::filesystem::path getHomeDirectory() {
    if (const char* homeDir = std::getenv("HOME")) {
        return homeDir;
    }
    fatal("failed to get home directory path");
}

std::filesystem::path getPlistPath() {
    auto home = getHomeDirectory();
    if (home.empty()) {
        fatal("failed to get plist path");
    }
    return home / "Library" / "LaunchAgents" / std::format("{}.plist", PLIST_NAME);
}

std::string getPlistContents() {
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

int launchctl(const std::vector<std::string>& args, bool suppressOutput) {
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
    if (suppressOutput) {
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

void ensureLaunchctl(const std::vector<std::string>& args, std::string_view action, bool suppressOutput = false) {
    const int status = launchctl(args, suppressOutput);
    if (status != 0) {
        fatal("launchctl {} failed with exit code {}", action, status);
    }
}

std::string getDomainTarget() {
    static const std::string target = std::format("gui/{}", getuid());
    return target;
}

std::string getServiceTarget() {
    static const std::string target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    return target;
}

bool isServiceBootstrapped() {
    auto result = launchctl({"blame", getServiceTarget()}, true);
    return result == 0;
}
}  // namespace

void service::install() {
    auto plistPath = getPlistPath();
    if (std::filesystem::exists(plistPath)) {
        fatal("service file '{}' is already installed", plistPath.string());
    }
    auto plistContents = getPlistContents();
    std::filesystem::create_directories(plistPath.parent_path());
    std::ofstream file(plistPath);
    if (!file) {
        fatal("failed to open '{}' for writing", plistPath.string());
    }
    file << plistContents;
    if (!file) {
        fatal("failed to write to '{}'", plistPath.string());
    }
    info("service installed");
}

void service::uninstall() {
    auto plistPath = getPlistPath();
    if (!std::filesystem::exists(plistPath)) {
        fatal("service file '{}' is not installed", plistPath.string());
    }
    std::error_code ec;
    std::filesystem::remove(plistPath, ec);
    if (ec) {
        fatal("failed to remove service file '{}': {}", plistPath.string(), ec.message());
    }
    info("service uninstalled");
}

void service::start() {
    auto plistPath = getPlistPath();
    bool installedDuringStart = false;
    if (!std::filesystem::exists(plistPath)) {
        warn("service file '{}' does not exist, installing", plistPath.string());
        install();
        installedDuringStart = true;
    }
    auto serviceTarget = getServiceTarget();
    auto domainTarget = getDomainTarget();
    const bool wasBootstrapped = isServiceBootstrapped();
    if (wasBootstrapped) {
        info("service already started");
        return;
    }

    ensureLaunchctl({"enable", serviceTarget}, "enable", true);
    ensureLaunchctl({"bootstrap", domainTarget, plistPath.string()}, "bootstrap", true);

    if (!isServiceBootstrapped()) {
        fatal("service did not appear in launchctl after start");
    }

    if (installedDuringStart) {
        info("service installed and started");
    } else {
        info("service started");
    }
}

void service::restart() {
    auto plistPath = getPlistPath();
    if (!std::filesystem::exists(plistPath)) {
        fatal("service file '{}' is not installed", plistPath.string());
    }
    ensureLaunchctl({"kickstart", "-k", getServiceTarget()}, "restart");
    info("service restarted");
}

void service::stop() {
    auto plistPath = getPlistPath();
    if (!std::filesystem::exists(plistPath)) {
        fatal("service file '{}' is not installed", plistPath.string());
    }
    auto serviceTarget = getServiceTarget();
    auto domainTarget = getDomainTarget();
    if (!isServiceBootstrapped()) {
        warn("service is not loaded");
        return;
    }

    ensureLaunchctl({"bootout", domainTarget, plistPath.string()}, "bootout", true);
    ensureLaunchctl({"disable", serviceTarget}, "disable", true);

    info("service stopped");
}
