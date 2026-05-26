#pragma once

#include "../input/chord.hpp"
#include "../input/modifier.hpp"
#include "../input/log.hpp"
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <print>
#include <string>



class KeyObserverHandler {
   private:
    CFRunLoopRef runLoop{};
    CFMachPortRef eventTap{};

    // exit chord: ctrl + c
    static constexpr Chord EXIT_CHORD{
        .keysym = {.keycode = 8},
        .modifiers = {.flags = Hotkey_Flag_Control}};

    bool setupEventTap();

    static CGEventRef eventCallback(
        CGEventTapProxy /*proxy*/, CGEventType type, CGEventRef event, void* /*refcon*/);

    static bool handleKeyEvent(CGEventRef event, CGEventType type);

   public:
    bool init();

    void run() const;
};
