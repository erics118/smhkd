#pragma once

#include <sys/stat.h>

#include <optional>
#include <string>

#include "hotkey.hpp"

[[nodiscard]] bool file_exists(const std::string& filename);

[[nodiscard]] std::optional<std::string> get_config_file(const std::string& name);

[[nodiscard]] bool has_flags(const Hotkey& hotkey, uint32_t flag);

void executeCommand(const std::string& command);
