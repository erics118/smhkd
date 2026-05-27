#include "process.hpp"

#include <format>

#include "../common/log.hpp"

pid_t read_pid_file() {
    const auto* user = std::getenv("USER");
    if (!user) {
        error("could not create path to pid file because 'env USER' is not set");
        std::exit(1);
    }
    auto pid_file = std::format(PIDFILE_FMT, user);
    int handle = open(pid_file.c_str(), O_RDWR);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (handle == -1) {
        error_errno("could not open pid file '{}'", pid_file);
    }
    if (flock(handle, LOCK_EX | LOCK_NB) == 0) {
        close(handle);
        error("Could not locate existing instance");
        std::exit(1);
    }
    pid_t pid = 0;
    if (read(handle, &pid, sizeof(pid_t)) == -1) {
        close(handle);
        error_errno("could not read pid file '{}'", pid_file);
    }
    close(handle);
    return pid;
}

void create_pid_file() {
    const auto* user = std::getenv("USER");
    if (!user) {
        error("could not create path to pid file because 'env USER' is not set");
        std::exit(1);
    }
    auto pid_file = std::format(PIDFILE_FMT, user);
    pid_t pid = getpid();
    int handle = open(pid_file.c_str(), O_CREAT | O_RDWR, 0644);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (handle == -1) {
        error_errno("could not create pid file '{}'", pid_file);
    }
    struct flock lockfd = {.l_start = 0, .l_len = 0, .l_pid = pid, .l_type = F_WRLCK, .l_whence = SEEK_SET};
    if (fcntl(handle, F_SETLK, &lockfd) == -1) {  // NOLINT(cppcoreguidelines-pro-type-vararg)
        close(handle);
        error_errno("could not lock pid file '{}'", pid_file);
    }
    if (write(handle, &pid, sizeof(pid_t)) == -1) {
        close(handle);
        error_errno("could not write pid {} to file '{}'", pid, pid_file);
    }
    debug("created pid file: {}", pid_file);
}

bool check_privileges() {
    std::array<const void*, 1> keys = {kAXTrustedCheckOptionPrompt};
    std::array<const void*, 1> values = {kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        keys.data(),
        values.data(),
        keys.size(),
        &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    bool result = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
    return result;
}
