#pragma once

#include <sys/stat.h>

#include <optional>
#include <string>

#include "hotkey.hpp"

// check if a file exists
[[nodiscard]] bool file_exists(const std::string& filename);

// get the config file
[[nodiscard]] std::optional<std::string> get_config_file(const std::string& name);

// check if a hotkey has a given flag
[[nodiscard]] bool has_flags(const Hotkey& hotkey, uint32_t flag);

// execute a command
void executeCommand(const std::string& command);
