#include "application.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <csignal>

#include "../common/log.hpp"

Application* Application::instance_ = nullptr;

Application::Application(std::string configFile) : configFile_(std::move(configFile)) {}

Application::~Application() {
    for (int fd : reloadSignalPipe_) {
        if (fd != -1) {
            close(fd);
        }
    }
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void Application::run() {
    keyHandler_ = std::make_unique<KeyHandler>(configFile_);
    if (!keyHandler_->init()) {
        error("failed to initialize key handler");
    }

    reloadContext_.handler = keyHandler_.get();
    instance_ = this;
    installSignalHandlers();
    setupReloadSignalSource(CFRunLoopGetCurrent());
    keyHandler_->run();
}

void Application::sigusr1Handler(int /*signal*/) {
    if (!instance_ || instance_->reloadSignalPipe_[1] == -1) {
        return;
    }
    const char byte = '\n';
    const ssize_t result = write(instance_->reloadSignalPipe_[1], &byte, sizeof(byte));
    (void)result;
}

void Application::reloadSignalCallback(CFFileDescriptorRef fd, CFOptionFlags /*callbackTypes*/, void* info) {
    auto* context = static_cast<ReloadContext*>(info);
    if (!context || !context->handler || !instance_) {
        return;
    }
    instance_->handleReloadSignal(fd);
}

void Application::installSignalHandlers() const {
    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, sigusr1Handler);
}

void Application::setupReloadSignalSource(CFRunLoopRef runLoop) {
    if (pipe(reloadSignalPipe_.data()) == -1) {
        error_errno("failed to create reload signal pipe");
    }

    for (int fd : reloadSignalPipe_) {
        const int flags = fcntl(fd, F_GETFL);  // NOLINT(cppcoreguidelines-pro-type-vararg)
        if (flags == -1) {
            error_errno("failed to read reload signal pipe flags");
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {  // NOLINT(cppcoreguidelines-pro-type-vararg)
            error_errno("failed to set reload signal pipe non-blocking");
        }
    }

    CFFileDescriptorContext fdContext{};
    fdContext.info = &reloadContext_;
    auto* reloadFd = CFFileDescriptorCreate(
        kCFAllocatorDefault,
        reloadSignalPipe_[0],
        false,
        reloadSignalCallback,
        &fdContext);
    if (!reloadFd) {
        error("failed to create reload signal file descriptor");
    }

    CFFileDescriptorEnableCallBacks(reloadFd, kCFFileDescriptorReadCallBack);
    CFRunLoopSourceRef source = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, reloadFd, 0);
    if (!source) {
        CFRelease(reloadFd);
        error("failed to create reload signal run loop source");
    }

    CFRunLoopAddSource(runLoop, source, kCFRunLoopCommonModes);
    CFRelease(source);
    CFRelease(reloadFd);
}

void Application::handleReloadSignal(CFFileDescriptorRef fd) {
    std::array<char, 64> buffer{};
    while (read(reloadSignalPipe_[0], buffer.data(), buffer.size()) > 0) {
    }

    debug("SIGUSR1 received, reloading config");
    keyHandler_->reload();
    CFFileDescriptorEnableCallBacks(fd, kCFFileDescriptorReadCallBack);
}
