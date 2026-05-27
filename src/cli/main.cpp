#include <print>
#include <span>
#include <string>

#ifndef SMHKD_VERSION
#define SMHKD_VERSION "unknown"
#endif

#include "../common/config_path.hpp"
#include "../common/log.hpp"
#include "../input/locale.hpp"
#include "../lang/config_loader.hpp"
#include "../runtime/key_observer_handler.hpp"
#include "../runtime/process.hpp"
#include "../runtime/service.hpp"
#include "application.hpp"
#include "cli.hpp"

namespace {

std::string parse_arguments(std::span<char* const> argv) {
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

    std::vector<std::string> argVector;
    argVector.reserve(argv.size());
    for (const auto* arg : argv) {
        argVector.emplace_back(arg);
    }
    Args args = parse_args(argVector, config);

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
        exit(0);
    }

    if (args.get('r', "reload")) {
        pid_t pid = read_pid_file();
        if (pid) kill(pid, SIGUSR1);
        info("config reloaded");
        exit(0);
    }

    std::string config_file = args.get('c', "config").value_or(get_config_file("smhkd").value_or(""));

    validate_config_file(config_file);

    if (args.get("dump-ast")) {
        auto result = ConfigLoader::loadFromFile(config_file);
        if (result.fileError) {
            warn("config error: {}", *result.fileError);
        }
        for (const auto& parse_error : result.parseErrors) {
            warn("parse error at line {}, column {}: {}", parse_error.row, parse_error.col, parse_error.message);
        }
        for (const auto& interpreter_error : result.interpreterErrors) {
            warn("config error: {}", interpreter_error.message);
        }
        std::print("{}\n", result.program);
        exit(0);
    }

    return config_file;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (getuid() == 0 || geteuid() == 0) {
        error("running as root is not allowed");
    }

    if (!initializeKeycodeMap()) {
        error("failed to initialize keycode map");
    }

    const std::string config_file = parse_arguments(std::span<char* const>(argv, static_cast<size_t>(argc)));

    create_pid_file();

    if (!check_privileges()) {
        error("must run with accessibility access");
    }

    Application app(config_file);
    app.run();
}
