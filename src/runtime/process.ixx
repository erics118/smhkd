module;

#include <Carbon/Carbon.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
#include <string>

export module process;
import log;

export pid_t read_pid_file();
export void create_pid_file();
export bool check_privileges();

constexpr std::string_view PIDFILE_FMT = "/tmp/smhkd_{}.pid";

export pid_t read_pid_file() {
    const auto* user = std::getenv("USER");
    if (!user) {
        error("could not create path to pid file because 'env USER' is not set");
        std::exit(1);
    }
    auto pid_file = std::format(PIDFILE_FMT, user);
    int handle = open(pid_file.c_str(), O_RDWR);
    if (handle == -1) {
        error("could not open pid file '{}'", pid_file);
        std::exit(1);
    }
    if (flock(handle, LOCK_EX | LOCK_NB) == 0) {
        close(handle);
        error("Could not locate existing instance");
        std::exit(1);
    }
    pid_t pid = 0;
    if (read(handle, &pid, sizeof(pid_t)) == -1) {
        close(handle);
        error("could not read pid file '{}'", pid_file);
        std::exit(1);
    }
    close(handle);
    return pid;
}

export void create_pid_file() {
    const auto* user = std::getenv("USER");
    if (!user) {
        error("could not create path to pid file because 'env USER' is not set");
        std::exit(1);
    }
    auto pid_file = std::format(PIDFILE_FMT, user);
    pid_t pid = getpid();
    int handle = open(pid_file.c_str(), O_CREAT | O_RDWR, 0644);
    if (handle == -1) {
        error("could not create pid file '{}'", pid_file);
        std::exit(1);
    }
    struct flock lockfd = {.l_start = 0, .l_len = 0, .l_pid = pid, .l_type = F_WRLCK, .l_whence = SEEK_SET};
    if (fcntl(handle, F_SETLK, &lockfd) == -1) {
        close(handle);
        error("could not lock pid file '{}'", pid_file);
    }
    if (write(handle, &pid, sizeof(pid_t)) == -1) {
        close(handle);
        error("could not write pid {} to file '{}'", pid, pid_file);
    }
    info("created pid file: {}", pid_file);
}

export bool check_privileges() {
    std::array<CFStringRef, 1> keys = {kAXTrustedCheckOptionPrompt};
    std::array<CFTypeRef, 1> values = {kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        reinterpret_cast<const void**>(keys.data()),
        reinterpret_cast<const void**>(values.data()),
        keys.size(),
        &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    bool result = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
    return result;
}
