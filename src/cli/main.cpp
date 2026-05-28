#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef SMHKD_VERSION
#define SMHKD_VERSION "unknown"
#endif

#include "../common/config_path.hpp"
#include "../common/log.hpp"
#include "../input/chord.hpp"
#include "../input/locale.hpp"
#include "../lang/config_loader.hpp"
#include "../lang/interpreter.hpp"
#include "../lang/parser.hpp"
#include "../runtime/hotkey_engine.hpp"
#include "../runtime/key_observer_handler.hpp"
#include "../runtime/process.hpp"
#include "../runtime/service.hpp"
#include "application.hpp"
#include "cli.hpp"

namespace {

Chord parseCliKeypress(std::string_view spec) {
    if (spec.empty()) {
        fatal("empty key spec");
    }

    Parser parser{std::string{spec}};
    auto chord = parser.parseChord();
    for (const auto& parse_error : parser.errors()) {
        error("invalid key spec at line {}, column {}: {}", parse_error.row, parse_error.col, parse_error.message);
    }
    if (!chord) {
        fatal("invalid key spec");
    }

    auto result = interpretChord(*chord);

    for (const auto& interpreter_error : result.errors) {
        error("invalid key spec: {}", interpreter_error.message);
    }
    if (!result.chord) {
        fatal("invalid key spec");
    }
    return *result.chord;
}

std::filesystem::path parseArguments(std::span<char* const> argv) {
    cli::Config config{
        .short_args = {"c:", "k:", "r", "o", "v"},
        .long_args = {
            "config:",
            "key:",
            "reload",
            "observe",
            "verbose",
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
    const auto userArgs = argv.subspan(std::min<size_t>(1, argv.size()));
    argVector.reserve(userArgs.size());
    for (const auto* arg : userArgs) {
        argVector.emplace_back(arg);
    }
    cli::Args args = parseArgs(argVector, config);
    setVerboseLogging(args.contains('v') || args.contains("verbose"));

    if (args.get("version")) {
        std::print("smhkd-v{}\n", SMHKD_VERSION);
        exit(0);
    }

    if (auto keySpec = args.get('k', "key")) {
        const Chord chord = parseCliKeypress(*keySpec);
        HotkeyEngine::synthesizeKeyPress(chord);
        exit(0);
    }

    if (args.get("install-service")) {
        service::install();
        exit(0);
    }

    if (args.get("uninstall-service")) {
        service::uninstall();
        exit(0);
    }

    if (args.get("start-service")) {
        service::start();
        exit(0);
    }

    if (args.get("stop-service")) {
        service::stop();
        exit(0);
    }

    if (args.get("restart-service")) {
        service::restart();
        exit(0);
    }

    if (args.get('o', "observe")) {
        KeyObserverHandler observer;
        if (!observer.init()) {
            fatal("failed to initialize observer");
        }
        observer.run();
        exit(0);
    }

    if (args.get('r', "reload")) {
        pid_t pid = readPidFile();
        if (pid) kill(pid, SIGUSR1);
        info("config reloaded");
        exit(0);
    }

    std::filesystem::path config_file =
        args.get('c', "config")
            .transform([](std::string value) { return std::filesystem::path{std::move(value)}; })
            .value_or(getConfigFile("smhkd").value_or(std::filesystem::path{}));

    ensureConfigFile(config_file);

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
        fatal("running as root is not allowed");
    }

    if (!initializeKeycodeMap()) {
        fatal("failed to initialize keycode map");
    }

    const std::filesystem::path config_file = parseArguments(std::span<char* const>(argv, static_cast<size_t>(argc)));

    createPidFile();

    if (!checkPrivileges()) {
        fatal("must run with accessibility access");
    }

    Application app(config_file);
    app.run();
}
