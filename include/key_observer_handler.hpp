#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

struct KeyObserverHandler {
    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    // initialize the key handler
    bool init();

    // run the key handler
    void run() const;

    // setup the event tap
    bool setupEventTap();

    // event callback
    [[nodiscard]] static CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon);

    // handle a key event
    [[nodiscard]] bool handleKeyEvent(CGEventRef event, CGEventType type);
};
