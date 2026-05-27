#pragma once

// based off of: https://github.com/koekeishiya/skhd/blob/2c9d3b9c9440797cd80856b6a54ec8817a6f3d19/src/locale.c#L88

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <optional>
#include <string>
#include <string_view>

using Keycode = uint32_t;

bool initializeKeycodeMap();
std::optional<Keycode> lookupKeycode(std::string_view key);
std::optional<std::string> lookupKeyString(Keycode keycode);
