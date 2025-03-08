#pragma once

#include <sys/stat.h>

#include <optional>
#include <string>

// check if a file exists
[[nodiscard]] bool file_exists(const std::string& filename);

// get the config file
[[nodiscard]] std::optional<std::string> get_config_file(const std::string& name);

// execute a command
void executeCommand(const std::string& command);
