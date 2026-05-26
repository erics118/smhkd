#pragma once

#include <optional>
#include <string>

void validate_config_file(const std::string& config_file);

[[nodiscard]] std::optional<std::string> get_config_file(const std::string& name);
