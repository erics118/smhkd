#include <fstream>

#include "cli.hpp"
#include "log.hpp"
#include "parser.hpp"
#include "service.hpp"
#include "utils.hpp"

#define PIDFILE_FMT "/tmp/smhkd_%s.pid"

Service* service = nullptr;
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
    bool result;
    const void* keys[] = {kAXTrustedCheckOptionPrompt};
    const void* values[] = {kCFBooleanTrue};

    CFDictionaryRef options;
    options = CFDictionaryCreate(kCFAllocatorDefault,
        keys, values, sizeof(keys) / sizeof(*keys),
        &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    result = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);

    return result;
}


#include <spawn.h>
#include <sys/stat.h>
#include <libproc.h>

#define MAXLEN 512

#define PATH_LAUNCHCTL   "/bin/launchctl"
#define NAME_SKHD_PLIST "com.erics118.smhkd"
#define PATH_SKHD_PLIST "%s/Library/LaunchAgents/" NAME_SKHD_PLIST ".plist"

#define SKHD_PLIST \
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n" \
    "<plist version=\"1.0\">\n" \
    "<dict>\n" \
    "    <key>Label</key>\n" \
    "    <string>" NAME_SKHD_PLIST "</string>\n" \
    "    <key>ProgramArguments</key>\n" \
    "    <array>\n" \
    "        <string>%s</string>\n" \
    "    </array>\n" \
    "    <key>EnvironmentVariables</key>\n" \
    "    <dict>\n" \
    "        <key>PATH</key>\n" \
    "        <string>%s</string>\n" \
    "    </dict>\n" \
    "    <key>RunAtLoad</key>\n" \
    "    <true/>\n" \
    "    <key>KeepAlive</key>\n" \
    "    <dict>\n" \
    "        <key>SuccessfulExit</key>\n" \
    " 	     <false/>\n" \
    " 	     <key>Crashed</key>\n" \
    " 	     <true/>\n" \
    "    </dict>\n" \
    "    <key>StandardOutPath</key>\n" \
    "    <string>/tmp/smhkd_%s.out.log</string>\n" \
    "    <key>StandardErrorPath</key>\n" \
    "    <string>/tmp/smhkd_%s.err.log</string>\n" \
    "    <key>ProcessType</key>\n" \
    "    <string>Interactive</string>\n" \
    "    <key>Nice</key>\n" \
    "    <integer>-20</integer>\n" \
    "</dict>\n" \
    "</plist>"

//
// NOTE(koekeishiya): A launchd service has the following states:
//
//          1. Installed / Uninstalled
//          2. Active (Enable / Disable)
//          3. Bootstrapped (Load / Unload)
//          4. Running (Start / Stop)
//

static int safe_exec(char *const argv[], bool suppress_output)
{
    pid_t pid;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    if (suppress_output) {
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY|O_APPEND, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY|O_APPEND, 0);
    }

    int status = posix_spawn(&pid, argv[0], &actions, NULL, argv, NULL);
    if (status) return 1;

    while ((waitpid(pid, &status, 0) == -1) && (errno == EINTR)) {
        usleep(1000);
    }

    if (WIFSIGNALED(status)) {
        return 1;
    } else if (WIFSTOPPED(status)) {
        return 1;
    } else {
        return WEXITSTATUS(status);
    }
}

static inline char *cfstring_copy(CFStringRef string)
{
    CFIndex num_bytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string), kCFStringEncodingUTF8);
    char *result = (char *)malloc(num_bytes + 1);
    if (!result) return NULL;

    if (!CFStringGetCString(string, result, num_bytes + 1, kCFStringEncodingUTF8)) {
        free(result);
        result = NULL;
    }

    return result;
}

extern "C" CFURLRef CFCopyHomeDirectoryURLForUser(void *user);
static char *populate_plist_path(void)
{
    CFURLRef homeurl_ref = CFCopyHomeDirectoryURLForUser(NULL);
    CFStringRef home_ref = homeurl_ref ? CFURLCopyFileSystemPath(homeurl_ref, kCFURLPOSIXPathStyle) : NULL;
    char *home = home_ref ? cfstring_copy(home_ref) : NULL;

    if (!home) {
        error("skhd: unable to retrieve home directory! abort..\n");
    }

    int size = strlen(PATH_SKHD_PLIST)-2 + strlen(home) + 1;
    char *result = (char *)malloc(size);
    if (!result) {
        error("skhd: could not allocate memory for plist path! abort..\n");
    }

    memset(result, 0, size);
    snprintf(result, size, PATH_SKHD_PLIST, home);

    return result;
}

static char *populate_plist(int *length)
{
    char *user = getenv("USER");
    if (!user) {
        error("skhd: 'env USER' not set! abort..\n");
    }

    char *path_env = getenv("PATH");
    if (!path_env) {
        error("skhd: 'env PATH' not set! abort..\n");
    }

    char exe_path[4096];
    unsigned int exe_path_size = sizeof(exe_path);
    pid_t pid = getpid();
    if (proc_pidpath(pid, exe_path, exe_path_size) <= 0) {
        error("skhd: unable to retrieve path of executable! abort..\n");
    }

    int size = strlen(SKHD_PLIST)-8 + strlen(exe_path) + strlen(path_env) + (2*strlen(user)) + 1;
    char *result = (char *)malloc(size);
    if (!result) {
        error("skhd: could not allocate memory for plist contents! abort..\n");
    }

    memset(result, 0, size);
    snprintf(result, size, SKHD_PLIST, exe_path, path_env, user, user);
    *length = size-1;

    return result;
}


static inline bool directory_exists(char *filename)
{
    struct stat buffer;

    if (stat(filename, &buffer) != 0) {
        return false;
    }

    return S_ISDIR(buffer.st_mode);
}

static inline void ensure_directory_exists(char *skhd_plist_path)
{
    //
    // NOTE(koekeishiya): Temporarily remove filename.
    // We know the filepath will contain a slash, as
    // it is controlled by us, so don't bother checking
    // the result..
    //

    char *last_slash = strrchr(skhd_plist_path, '/');
    *last_slash = '\0';

    if (!directory_exists(skhd_plist_path)) {
        mkdir(skhd_plist_path, 0755);
    }

    //
    // NOTE(koekeishiya): Restore original filename.
    //

    *last_slash = '/';
}

static int service_install_internal(char *skhd_plist_path)
{
    int skhd_plist_length;
    char *skhd_plist = populate_plist(&skhd_plist_length);
    ensure_directory_exists(skhd_plist_path);

    FILE *handle = fopen(skhd_plist_path, "w");
    if (!handle) return 1;

    size_t bytes = fwrite(skhd_plist, skhd_plist_length, 1, handle);
    int result = bytes == 1 ? 0 : 1;
    fclose(handle);

    return result;
}

static int service_install(void)
{
    char *skhd_plist_path = populate_plist_path();

    if (file_exists(skhd_plist_path)) {
        error("skhd: service file '%s' is already installed! abort..\n", skhd_plist_path);
    }

    return service_install_internal(skhd_plist_path);
}

static int service_uninstall(void)
{
    char *skhd_plist_path = populate_plist_path();

    if (!file_exists(skhd_plist_path)) {
        error("skhd: service file '%s' is not installed! abort..\n", skhd_plist_path);
    }

    return unlink(skhd_plist_path) == 0 ? 0 : 1;
}

static int service_start(void)
{
    char *skhd_plist_path = populate_plist_path();
    if (!file_exists(skhd_plist_path)) {
        warn("skhd: service file '%s' is not installed! attempting installation..\n", skhd_plist_path);

        int result = service_install_internal(skhd_plist_path);
        if (result) {
            error("skhd: service file '%s' could not be installed! abort..\n", skhd_plist_path);
        }
    }

    char service_target[MAXLEN];
    snprintf(service_target, sizeof(service_target), "gui/%d/%s", getuid(), NAME_SKHD_PLIST);

    char domain_target[MAXLEN];
    snprintf(domain_target, sizeof(domain_target), "gui/%d", getuid());

    //
    // NOTE(koekeishiya): Check if service is bootstrapped
    //

    const char *const args[] = { PATH_LAUNCHCTL, "print", service_target, NULL };
    int is_bootstrapped = safe_exec((char *const*)args, true);

    if (is_bootstrapped != 0) {

        //
        // NOTE(koekeishiya): Service is not bootstrapped and could be disabled.
        // There is no way to query if the service is disabled, and we cannot
        // bootstrap a disabled service. Try to enable the service. This will be
        // a no-op if the service is already enabled.
        //

        const char *const args[] = { PATH_LAUNCHCTL, "enable", service_target, NULL };
        safe_exec((char *const*)args, false);

        //
        // NOTE(koekeishiya): Bootstrap service into the target domain.
        // This will also start the program **iff* RunAtLoad is set to true.
        //

        const char *const args2[] = { PATH_LAUNCHCTL, "bootstrap", domain_target, skhd_plist_path, NULL };
        return safe_exec((char *const*)args2, false);
    } else {

        //
        // NOTE(koekeishiya): The service has already been bootstrapped.
        // Tell the bootstrapped service to launch immediately; it is an
        // error to bootstrap a service that has already been bootstrapped.
        //

        const char *const args[] = { PATH_LAUNCHCTL, "kickstart", service_target, NULL };
        return safe_exec((char *const*)args, false);
    }
}

static int service_restart(void)
{
    char *skhd_plist_path = populate_plist_path();
    if (!file_exists(skhd_plist_path)) {
        error("skhd: service file '%s' is not installed! abort..\n", skhd_plist_path);
    }

    char service_target[MAXLEN];
    snprintf(service_target, sizeof(service_target), "gui/%d/%s", getuid(), NAME_SKHD_PLIST);

    const char *const args[] = { PATH_LAUNCHCTL, "kickstart", "-k", service_target, NULL };
    return safe_exec((char *const*)args, false);
}

static int service_stop(void)
{
    char *skhd_plist_path = populate_plist_path();
    if (!file_exists(skhd_plist_path)) {
        error("skhd: service file '%s' is not installed! abort..\n", skhd_plist_path);
    }

    char service_target[MAXLEN];
    snprintf(service_target, sizeof(service_target), "gui/%d/%s", getuid(), NAME_SKHD_PLIST);

    char domain_target[MAXLEN];
    snprintf(domain_target, sizeof(domain_target), "gui/%d", getuid());

    //
    // NOTE(koekeishiya): Check if service is bootstrapped
    //

    const char *const args[] = { PATH_LAUNCHCTL, "print", service_target, NULL };
    int is_bootstrapped = safe_exec((char *const*)args, true);

    if (is_bootstrapped != 0) {

        //
        // NOTE(koekeishiya): Service is not bootstrapped, but the program
        // could still be running an instance that was started **while the service
        // was bootstrapped**, so we tell it to stop said service.
        //

        const char *const args[] = { PATH_LAUNCHCTL, "kill", "SIGTERM", service_target, NULL };
        return safe_exec((char *const*)args, false);
    } else {

        //
        // NOTE(koekeishiya): Service is bootstrapped; we stop a potentially
        // running instance of the program and unload the service, making it
        // not trigger automatically in the future.
        //
        // This is NOT the same as disabling the service, which will prevent
        // it from being boostrapped in the future (without explicitly re-enabling
        // it first).
        //

        const char *const args[] = { PATH_LAUNCHCTL, "bootout", domain_target, skhd_plist_path, NULL };
        return safe_exec((char *const*)args, false);
    }
}



int main(int argc, char* argv[]) {
    ArgsConfig config{
        .short_args = {"c:", "r"},
        .long_args = {"config:", "reload", "install-service", "uninstall-service", "start-service", "stop-service", "restart-service"},
    };
    Args args = parse_args(std::vector<std::string>(argv, argv + argc), config);

    if (args.get("install-service")) {
        service_install();
        return 0;
    }

    if (args.get("uninstall-service")) {
        service_uninstall();
        return 0;
    }

    if (args.get("start-service")) {
        service_start();
        return 0;
    }

    if (args.get("stop-service")) {
        service_stop();
        return 0;
    }

    if (args.get("restart-service")) {
        service_restart();
        return 0;
    }

    if (args.get('r', "reload")) {
        info("reloading config");
        pid_t pid = read_pid_file();
        if (pid) kill(pid, SIGUSR1);
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

    config_file = args.get('c', "config").value_or(get_config_file("smhkd").value_or(""));

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

        service = new Service(hotkeys);

        service->init();

        service->run();

    } catch (const std::exception& ex) {
        error("Error while parsing hotkeys: {}", ex.what());
    }
}
