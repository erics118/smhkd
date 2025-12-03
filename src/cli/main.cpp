#include <CoreFoundation/CoreFoundation.h>

#include <fstream>
#include <print>

#ifndef SMHKD_VERSION
#define SMHKD_VERSION "unknown"
#endif

import smhkd.utils;
import smhkd.cli;
import smhkd.process;
import smhkd.service;
import smhkd.parser;
import smhkd.ast_printer;
import smhkd.key_handler;
import smhkd.key_observer_handler;
import smhkd.log;
import smhkd.ast;
import smhkd.locale;
import smhkd.key_handler;

KeyHandler* service = nullptr;
std::string config_file;

static void sigusr1_handler(int /*signal*/) {
    debug("SIGUSR1 received.. reloading config");
    service->reload();
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
        smhkd::key_observer_handler::KeyObserverHandler observer;
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
        debug(config_file);
    }

    if (args.get("dump-ast")) {
        if (!file_exists(config_file)) error("config file not found");
        if (config_file.empty()) error("config file empty");

        std::ifstream file(config_file);
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Parser p(contents);
        ast::Program prog = p.parseProgram();
        auto dumped = dump_ast(prog);
        std::print("{}", dumped);
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

    try {
        parse_arguments(argc, argv);
    } catch (const std::exception& ex) {
        error(ex.what());
    }

    create_pid_file();

    if (!check_privileges()) {
        error("must run with accessibility access");
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, sigusr1_handler);

    try {
        service = new KeyHandler(config_file);
        service->init();
        service->run();
    } catch (const std::exception& ex) {
        error("Error while parsing hotkeys: {}", ex.what());
    }
}
