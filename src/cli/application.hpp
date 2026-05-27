#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include <array>
#include <memory>
#include <string>

#include "../runtime/key_handler.hpp"

class Application {
   public:
    explicit Application(std::string configFile);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();

   private:
    struct ReloadContext {
        KeyHandler* handler{};
    };

    std::string configFile_;
    std::unique_ptr<KeyHandler> keyHandler_;
    ReloadContext reloadContext_{};
    std::array<int, 2> reloadSignalPipe_ = {-1, -1};

    static Application* instance_;

    static void sigusr1Handler(int signal);
    static void reloadSignalCallback(CFFileDescriptorRef fd, CFOptionFlags callbackTypes, void* info);

    void installSignalHandlers() const;
    void setupReloadSignalSource(CFRunLoopRef runLoop);
    void handleReloadSignal(CFFileDescriptorRef fd);
};
