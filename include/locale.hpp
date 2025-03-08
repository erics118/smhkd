#pragma once

#include <string>
#include <unordered_map>

using Keycode = uint32_t;

// keycode mapping
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline std::unordered_map<std::string, Keycode> keycodeMap;

bool initializeKeycodeMap();

// get the keycode for a given key
[[nodiscard]] uint32_t getKeycode(const std::string& key);

// get the name of a given keycode
[[nodiscard]] std::string getNameOfKeycode(Keycode keycode);
