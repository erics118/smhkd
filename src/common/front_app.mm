#include "front_app.hpp"

#import <AppKit/AppKit.h>

#include <mutex>
#include <string>

#include "string_util.hpp"

namespace {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

// lowercased frontmost app name, updated on app activation
std::string gCachedName;

// mutex to protect gCachedName
std::mutex gMutex;

// keep the observer token alive for process lifetime
id gObserverToken = nil;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

void storeName(NSString* name) {
    const char* utf8 = name.UTF8String;
    std::string lower = utf8 ? toLower(utf8) : std::string{};
    std::lock_guard lk(gMutex);
    gCachedName = std::move(lower);
}

void initOnce() {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      storeName(NSWorkspace.sharedWorkspace.frontmostApplication.localizedName);

      gObserverToken = [NSWorkspace.sharedWorkspace.notificationCenter
          addObserverForName:NSWorkspaceDidActivateApplicationNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* note) {
                    NSRunningApplication* app = note.userInfo[NSWorkspaceApplicationKey];
                    storeName(app.localizedName);
                  }];
    });
}

}  // namespace

std::string getFrontProcessName() {
    initOnce();
    std::lock_guard lk{gMutex};
    return gCachedName;
}
