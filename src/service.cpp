#include "service.hpp"

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
#include <vector>

#include "log.hpp"
#include "utils.hpp"

std::string get_home_directory() {
    CFURLRef homeurl_ref = CFCopyHomeDirectoryURLForUser(nullptr);
    if (!homeurl_ref) {
        throw std::runtime_error("failed to get home directory URL");
    }

    std::string home_ref = cfStringToString(CFURLCopyFileSystemPath(homeurl_ref, kCFURLPOSIXPathStyle));

    CFRelease(homeurl_ref);

    if (home_ref.empty()) throw std::runtime_error("failed to get home directory path");

    return home_ref;
}

std::string get_plist_path() {
    auto home = get_home_directory();
    if (home.empty()) throw std::runtime_error("failed to get plist path");

    return std::format("{}/Library/LaunchAgents/{}.plist", home, PLIST_NAME);
}

std::string get_plist_contents() {
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

    if (posix_spawn_file_actions_init(&actions) != 0) {
        throw std::runtime_error("failed to initialize spawn file actions");
    }

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
        throw std::runtime_error(std::format("Process terminated abnormally with status {}", status));
    }
    return 0;
}

void service_install() {
    auto plist_path = get_plist_path();

    if (std::filesystem::exists(plist_path)) {
        throw std::runtime_error(std::format("service file '{}' is already installed", plist_path));
    }

    auto plist_contents = get_plist_contents();

    // create directory if it doesn't exist
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

void service_uninstall() {
    auto plist_path = get_plist_path();

    if (!std::filesystem::exists(plist_path)) {
        throw std::runtime_error(std::format("service file '{}' is not installed", plist_path));
    }

    std::filesystem::remove(plist_path);
}

void service_start() {
    auto plist_path = get_plist_path();

    if (!std::filesystem::exists(plist_path)) {
        info("service file '{}' does not exist, installing", plist_path);

        service_install();
    }

    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    auto domain_target = std::format("gui/{}", getuid());

    // check if service is bootstrapped
    auto result = launchctl_exec({"print", service_target}, true);
    debug("result: {}", result);
    if (result != 0) {
        // not bootstrapped, try to enable it
        launchctl_exec({"enable", service_target});

        // bootstrap the service
        launchctl_exec({"bootstrap", domain_target, plist_path});
    } else {
        debug("{} is already bootstrapped", service_target);
        // bootstrapped, kickstart it
        launchctl_exec({"kickstart",  service_target});
    }
}

void service_restart() {
    auto plist_path = get_plist_path();

    if (!std::filesystem::exists(plist_path)) {
        throw std::runtime_error(std::format("service file '{}' is not installed", plist_path));
    }

    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    launchctl_exec({"kickstart", "-k", service_target});
}

void service_stop() {
    auto plist_path = get_plist_path();

    if (!std::filesystem::exists(plist_path)) {
        throw std::runtime_error(std::format("service file '{}' is not installed", plist_path));
    }

    auto service_target = std::format("gui/{}/{}", getuid(), PLIST_NAME);
    auto domain_target = std::format("gui/{}", getuid());

    // check if service is bootstrapped
    auto result = launchctl_exec({"print", service_target}, true);

    if (result != 0) {
        // not bootstrapped, just try to stop it
        launchctl_exec({"kill", "SIGTERM", service_target});
    } else {
        // bootstrapped, bootout it
        launchctl_exec({"bootout", domain_target, plist_path});

        // disable it
        // launchctl_exec({"disable", service_target});
    }
}
