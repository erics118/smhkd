#pragma once

#include "../utils.hpp"
// based off of: https://github.com/koekeishiya/skhd/blob/2c9d3b9c9440797cd80856b6a54ec8817a6f3d19/src/locale.c#L88

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include <array>
#include <format>
#include <memory>
#include <type_traits>
#include <unordered_map>


using Keycode = uint32_t;

// global keycode map
extern std::unordered_map<std::string, Keycode> keycodeMap;

// From keysym module/header
// literal arrays come from keysym

bool initializeKeycodeMap();
