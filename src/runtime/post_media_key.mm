#include "post_media_key.hpp"

#import <Cocoa/Cocoa.h>

void postMediaKey(int nxKeyType, bool keyDown) {
    const int state = keyDown ? 0xA : 0xB;
    NSEvent* event = [NSEvent otherEventWithType:NSEventTypeSystemDefined
                                        location:NSZeroPoint
                                   modifierFlags:static_cast<NSEventModifierFlags>(state << 8)
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:8
                                           data1:(nxKeyType << 16) | (state << 8)
                                           data2:-1];
    CGEventRef cg = [event CGEvent];
    if (cg) {
        CGEventPost(kCGHIDEventTap, cg);
    }
}
