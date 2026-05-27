#include "command.hpp"

#include <unistd.h>

#include <string>
#include <vector>

#include "log.hpp"

void executeCommand(const std::string& command) {
    pid_t cpid = fork();

    if (cpid < 0) {
        warn("failed to fork process for command execution");
        return;
    }

    if (cpid == 0) {
        setsid();

        std::vector<std::string> stringStorage = {"/bin/zsh", "-c", command};
        std::vector<char*> args;
        args.reserve(stringStorage.size());
        for (auto& str : stringStorage) {
            args.push_back(str.data());
        }
        args.push_back(nullptr);

        int status = execvp(args[0], args.data());
        warn("failed to execute command '{}'", command);
        _exit(status);
    }
}
