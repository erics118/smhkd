#include "front_app.hpp"

#import <AppKit/AppKit.h>

std::string getFrontProcessName() {
    NSRunningApplication* app = NSWorkspace.sharedWorkspace.frontmostApplication;
    if (!app) {
        return "";
    }
    NSString* name = app.localizedName;
    if (!name) {
        return "";
    }
    const char* utf8 = name.UTF8String;
    return utf8 ? std::string{utf8} : std::string{};
}
