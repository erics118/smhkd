#pragma once

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <string>
#include <unordered_map>

// Keycode mapping
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline std::unordered_map<std::string, uint32_t> keycodeMap;

bool initializeKeycodeMap();

static std::string cfStringToString(CFStringRef cfString);

[[nodiscard]] uint32_t getKeyCode(const std::string& key);

[[nodiscard]] std::string getCharFromKeycode(uint32_t keycode);
