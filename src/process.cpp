#include "process.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <format>
#include <string>

#include "log.hpp"

namespace {
constexpr std::string_view PIDFILE_FMT = "/tmp/smhkd_{}.pid";
}

pid_t read_pid_file() {
    const auto* user = std::getenv("USER");
    if (!user) {
        error("could not create path to pid file because 'env USER' was not set! abort");
    }

    auto pid_file = std::format(PIDFILE_FMT, user);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int handle = open(pid_file.c_str(), O_RDWR);
    if (handle == -1) {
        error("could not open pid file");
    }

    if (flock(handle, LOCK_EX | LOCK_NB) == 0) {
        error("could not locate existing instance");
    }

    pid_t pid = 0;
    if (read(handle, &pid, sizeof(pid_t)) == -1) {
        error("could not read pid file");
    }

    close(handle);
    return pid;
}

void create_pid_file() {
    const auto* user = std::getenv("USER");
    if (!user) {
        error("could not create path to pid file because 'env USER' was not set! abort");
    }

    auto pid_file = std::format(PIDFILE_FMT, user);
    pid_t pid = getpid();

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int handle = open(pid_file.c_str(), O_CREAT | O_RDWR, 0644);
    if (handle == -1) {
        error("could not create pid file");
    }

    struct flock lockfd = {
        .l_start = 0,
        .l_len = 0,
        .l_pid = pid,
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
    };
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    if (fcntl(handle, F_SETLK, &lockfd) == -1) {
        close(handle);
        error("could not lock pid file");
    }

    if (write(handle, &pid, sizeof(pid_t)) == -1) {
        close(handle);
        error("could not write pid file");
    }

    // NOTE(koekeishiya): we intentionally leave the handle open,
    // as calling close(..) will release the lock we just acquired.

    debug("successfully created pid file");
}

bool check_privileges() {
    std::array<CFStringRef, 1> keys = {kAXTrustedCheckOptionPrompt};
    std::array<CFTypeRef, 1> values = {kCFBooleanTrue};

    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<const void**>(keys.data()),
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<const void**>(values.data()),
        keys.size(),
        &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    bool result = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);

    return result;
}
