#pragma once

#include <CoreFoundation/CFBase.h>
#include <sys/stat.h>

#include <optional>
#include <string>

// convert a CFStringRef to a std::string
[[nodiscard]] std::string cfStringToString(CFStringRef cfString);

// check if a file exists
[[nodiscard]] bool file_exists(const std::string& filename);

// get the config file
[[nodiscard]] std::optional<std::string> get_config_file(const std::string& name);

// execute a command
void executeCommand(const std::string& command);
