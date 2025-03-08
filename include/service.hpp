#pragma once

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <libproc.h>
#include <spawn.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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

// Forward declaration of CFURLRef function
extern "C" CFURLRef CFCopyHomeDirectoryURLForUser(void* user);

std::string get_home_directory();

std::string get_plist_path();

std::string get_plist_contents();

void ensure_directory_exists(const std::filesystem::path& path);

int launchctl_exec(const std::vector<std::string>& args, bool suppress_output = false);

[[nodiscard]] std::string get_service_target();

[[nodiscard]] std::string get_domain_target();

[[nodiscard]] bool is_service_bootstrapped();

void service_install();

void service_uninstall();

void service_start();

void service_restart();

void service_stop();
