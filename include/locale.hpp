#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <string>
#include <unordered_map>

using Keycode = uint32_t;

// Keycode mapping
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline std::unordered_map<std::string, Keycode> keycodeMap;

bool initializeKeycodeMap();

static std::string cfStringToString(CFStringRef cfString);

[[nodiscard]] uint32_t getKeycode(const std::string& key);

[[nodiscard]] std::string getNameOfKeycode(Keycode keycode);
