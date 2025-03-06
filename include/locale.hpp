#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <string>
#include <unordered_map>

// Keycode mapping
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline std::unordered_map<std::string, uint32_t> keycodeMap;

bool initializeKeycodeMap();

// convert a CFStringRef to a std::string
static std::string cfStringToString(CFStringRef cfString);

// get the keycode for a given key
[[nodiscard]] uint32_t getKeycode(const std::string& key);
