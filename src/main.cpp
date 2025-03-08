#include <fstream>

#include "cli.hpp"
#include "key_handler.hpp"
#include "log.hpp"
#include "parser.hpp"
#include "service.hpp"
#include "utils.hpp"

#define PIDFILE_FMT "/tmp/smhkd_%s.pid"

KeyHandler* service = nullptr;
std::string config_file;

static void sigusr1_handler(int signal) {
    debug("SIGUSR1 received.. reloading config");

    std::ifstream file(config_file);

    // read entire file into string
    std::string configFileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    Parser parser(configFileContents);

    auto hotkeys = parser.parseFile();

    service->hotkeys = hotkeys;
}

static pid_t read_pid_file(void) {
    char pid_file[255] = {};
    pid_t pid = 0;

    char* user = getenv("USER");
    if (user) {
        snprintf(pid_file, sizeof(pid_file), PIDFILE_FMT, user);
    } else {
        error("could not create path to pid-file because 'env USER' was not set! abort..");
    }

    int handle = open(pid_file, O_RDWR);
    if (handle == -1) {
        error("could not open pid-file..");
    }

    if (flock(handle, LOCK_EX | LOCK_NB) == 0) {
        error("could not locate existing instance..");
    } else if (read(handle, &pid, sizeof(pid_t)) == -1) {
        error("could not read pid-file..");
    }

    close(handle);
    return pid;
}

static void create_pid_file(void) {
    char pid_file[255] = {};
    pid_t pid = getpid();

    char* user = getenv("USER");
    if (user) {
        snprintf(pid_file, sizeof(pid_file), PIDFILE_FMT, user);
    } else {
        error("could not create path to pid-file because 'env USER' was not set! abort..");
    }

    int handle = open(pid_file, O_CREAT | O_RDWR, 0644);
    if (handle == -1) {
        error("could not create pid-file! abort..");
    }

    struct flock lockfd = {
        .l_start = 0,
        .l_len = 0,
        .l_pid = pid,
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET};

    if (fcntl(handle, F_SETLK, &lockfd) == -1) {
        error("could not lock pid-file! abort..");
    } else if (write(handle, &pid, sizeof(pid_t)) == -1) {
        error("could not write pid-file! abort..");
    }

    // NOTE(koekeishiya): we intentionally leave the handle open,
    // as calling close(..) will release the lock we just acquired.

    debug("successfully created pid-file..");
}

static bool check_privileges(void) {
    // bool result;
    // const void* keys[] = {kAXTrustedCheckOptionPrompt};
    // const void* values[] = {kCFBooleanTrue};

    // CFDictionaryRef options;
    // options = CFDictionaryCreate(kCFAllocatorDefault,
    //     keys, values, sizeof(keys) / sizeof(*keys),
    //     &kCFCopyStringDictionaryKeyCallBacks,
    //     &kCFTypeDictionaryValueCallBacks);

    // result = AXIsProcessTrustedWithOptions(options);
    // CFRelease(options);

    bool result = AXIsProcessTrusted();

    return result;
}

bool parse_arguments(int argc, char* argv[]) {
    ArgsConfig config{
        .short_args = {"c:", "r"},
        .long_args = {"config:", "reload", "install-service", "uninstall-service", "start-service", "stop-service", "restart-service"},
    };
    Args args = parse_args(std::vector<std::string>(argv, argv + argc), config);

    if (args.get("install-service")) {
        if (auto result = service_install(); !result) {
            error("failed to install service: {}", result.error());
            return false;
        }
        return true;
    }

    if (args.get("uninstall-service")) {
        if (auto result = service_uninstall(); !result) {
            error("failed to uninstall service: {}", result.error());
            return false;
        }
        return true;
    }

    if (args.get("start-service")) {
        if (auto result = service_start(); !result) {
            error("failed to start service: {}", result.error());
            return false;
        }
        return true;
    }

    if (args.get("stop-service")) {
        if (auto result = service_stop(); !result) {
            error("failed to stop service: {}", result.error());
            return false;
        }
        return true;
    }

    if (args.get("restart-service")) {
        if (auto result = service_restart(); !result) {
            error("failed to restart service: {}", result.error());
            return false;
        }
        return true;
    }

    if (args.get('r', "reload")) {
        info("reloading config");
        pid_t pid = read_pid_file();
        if (pid) kill(pid, SIGUSR1);
        return true;
    }

    if (auto val = args.get('c', "config")) {
        config_file = *val;
    } else {
        config_file = get_config_file("smhkd").value_or("");
    }

    return false;
}

int main(int argc, char* argv[]) {
    if (getuid() == 0 || geteuid() == 0) {
        error("skhd: running as root is not allowed! abort");
        return 1;
    }
    if (parse_arguments(argc, argv)) {
        return 0;
    }

    create_pid_file();

    if (!check_privileges()) {
        error("must run with accessibility access");
        return 1;
    }

    if (!initializeKeycodeMap()) {
        error("failed to initialize keycode map");
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, sigusr1_handler);

    if (config_file.empty() || !file_exists(config_file)) {
        error("config file not found");
        return 1;
    }

    info("config file set to: {}", config_file);

    std::ifstream file(config_file);

    // read entire file into string
    std::string configFileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    try {
        Parser parser(configFileContents);

        auto hotkeys = parser.parseFile();

        service = new KeyHandler(hotkeys);

        service->init();

        service->run();

    } catch (const std::exception& ex) {
        error("Error while parsing hotkeys: {}", ex.what());
    }
}
