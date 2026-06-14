#include "command.hpp"

#include <dispatch/dispatch.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "log.hpp"

namespace {

void spawnCommand(const std::string& command) {
    pid_t cpid = fork();

    if (cpid < 0) {
        warn("failed to fork process for command execution");
        return;
    }

    if (cpid == 0) {
        setsid();

        // use bash without startup files, so especially PATH is inherited
        std::vector<std::string> stringStorage = {"/bin/bash", "--norc", "--noprofile", "-c", command};
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

}  // namespace

void executeCommand(std::string command) {
    // run the fork/exec on a background queue so event tap thread is never blocked by fork
    // capture command by value so it outlives this call (the block runs later)
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
        ^{
          spawnCommand(command);
        });
}
