#include "cli.hpp"
#include "key_handler.hpp"
#include "log.hpp"
#include "process.hpp"
#include "service.hpp"
#include "utils.hpp"

KeyHandler* service = nullptr;
std::string config_file;

static void sigusr1_handler(int /*signal*/) {
    debug("SIGUSR1 received.. reloading config");
    service->reload();
}

void parse_arguments(int argc, char* argv[]) {
    ArgsConfig config{
        .short_args = {"c:", "r"},
        .long_args = {
            "config:",
            "reload",
            "install-service",
            "uninstall-service",
            "start-service",
            "stop-service",
            "restart-service",
        },
    };
    Args args = parse_args(std::vector<std::string>(argv, argv + argc), config);

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
}

int main(int argc, char* argv[]) {
    if (getuid() == 0 || geteuid() == 0) {
        error("running as root is not allowed");
    }

    try {
        parse_arguments(argc, argv);
    } catch (const std::exception& ex) {
        error(ex.what());
    }

    create_pid_file();

    if (!check_privileges()) {
        error("must run with accessibility access");
    }

    if (!initializeKeycodeMap()) {
        error("failed to initialize keycode map");
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, sigusr1_handler);

    if (config_file.empty() || !file_exists(config_file)) {
        error("config file not found");
        return 1;
    }

    info("config file set to: {}", config_file);

    try {
        service = new KeyHandler(config_file);
        service->init();
        service->run();
    } catch (const std::exception& ex) {
        error("Error while parsing hotkeys: {}", ex.what());
    }
}
