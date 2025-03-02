#pragma once

#include <sys/stat.h>

#include <optional>
#include <string>

#include "hotkey.hpp"

bool file_exists(const std::string& filename);

std::optional<std::string> get_config_file(const std::string& name);
