#include <CoreFoundation/CoreFoundation.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <fstream>
#include <print>
#include <string>

#ifndef SMHKD_VERSION
#define SMHKD_VERSION "unknown"
#endif

#include "../common/config_path.hpp"
#include "../common/log.hpp"
#include "../input/locale.hpp"
#include "../lang/ast.hpp"
#include "../lang/parser.hpp"
#include "../runtime/key_handler.hpp"
#include "../runtime/key_observer_handler.hpp"
#include "../runtime/process.hpp"
#include "../runtime/service.hpp"
#include "cli.hpp"

KeyHandler* service = nullptr;
std::string config_file;
int reload_signal_pipe[2] = {-1, -1};

static void reload_signal_callback(CFFileDescriptorRef fd, CFOptionFlags /*callback_types*/, void* /*info*/) {
    char buffer[64];
    while (read(reload_signal_pipe[0], buffer, sizeof(buffer)) > 0) {
    }

    if (service) {
        debug("SIGUSR1 received, reloading config");
        service->reload();
    }

    CFFileDescriptorEnableCallBacks(fd, kCFFileDescriptorReadCallBack);
}

static void sigusr1_handler(int /*signal*/) {
    if (reload_signal_pipe[1] == -1) {
        return;
    }
    const char byte = '\n';
    const ssize_t result = write(reload_signal_pipe[1], &byte, sizeof(byte));
    (void)result;
}

static void setup_reload_signal_source(CFRunLoopRef runLoop) {
    if (pipe(reload_signal_pipe) == -1) {
        error_errno("failed to create reload signal pipe");
    }

    for (int fd : reload_signal_pipe) {
        const int flags = fcntl(fd, F_GETFL);
        if (flags == -1) {
            error_errno("failed to read reload signal pipe flags");
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            error_errno("failed to set reload signal pipe non-blocking");
        }
    }

    CFFileDescriptorContext context{};
    auto reload_fd = CFFileDescriptorCreate(
        kCFAllocatorDefault,
        reload_signal_pipe[0],
        false,
        reload_signal_callback,
        &context);
    if (!reload_fd) {
        error("failed to create reload signal file descriptor");
    }

    CFFileDescriptorEnableCallBacks(reload_fd, kCFFileDescriptorReadCallBack);
    CFRunLoopSourceRef source = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, reload_fd, 0);
    if (!source) {
        CFRelease(reload_fd);
        error("failed to create reload signal run loop source");
    }

    CFRunLoopAddSource(runLoop, source, kCFRunLoopCommonModes);
    CFRelease(source);
    CFRelease(reload_fd);
}

void parse_arguments(int argc, char* argv[]) {
    ArgsConfig config{
        .short_args = {"c:", "r", "o"},
        .long_args = {
            "config:",
            "reload",
            "observe",
            "install-service",
            "uninstall-service",
            "start-service",
            "stop-service",
            "restart-service",
            "dump-ast",
            "version",
        },
    };

    Args args = parse_args(std::vector<std::string>(argv, argv + argc), config);

    if (args.get("version")) {
        std::print("smhkd-v{}\n", SMHKD_VERSION);
        exit(0);
    }

    if (args.get("install-service")) {
        service_install();
        info("service installed");
        exit(0);
    }

    if (args.get("uninstall-service")) {
        service_uninstall();
        info("service uninstalled");
        exit(0);
    }

    if (args.get("start-service")) {
        service_start();
        info("service started");
        exit(0);
    }

    if (args.get("stop-service")) {
        service_stop();
        info("service stopped");
        exit(0);
    }

    if (args.get("restart-service")) {
        service_restart();
        info("service restarted");
        exit(0);
    }

    if (args.get('o', "observe")) {
        KeyObserverHandler observer;
        observer.init();
        observer.run();
    }

    if (args.get('r', "reload")) {
        pid_t pid = read_pid_file();
        if (pid) kill(pid, SIGUSR1);
        info("config reloaded");
        exit(0);
    }

    if (auto val = args.get('c', "config")) {
        config_file = *val;
    } else {
        config_file = get_config_file("smhkd").value_or("");
    }

    validate_config_file(config_file);

    if (args.get("dump-ast")) {
        std::ifstream file(config_file);
        const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Parser p(contents);
        ast::Program program = p.parseProgram();
        std::print("{}\n", program);
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    if (getuid() == 0 || geteuid() == 0) {
        error("running as root is not allowed");
    }

    if (!initializeKeycodeMap()) {
        error("failed to initialize keycode map");
    }

    parse_arguments(argc, argv);

    create_pid_file();

    if (!check_privileges()) {
        error("must run with accessibility access");
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, sigusr1_handler);

    service = new KeyHandler(config_file);
    service->init();
    setup_reload_signal_source(CFRunLoopGetCurrent());
    service->run();
}
