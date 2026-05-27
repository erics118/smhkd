#pragma once

#include <filesystem>
#include <optional>
#include <string>

void ensureConfigFile(const std::filesystem::path& configFile);

[[nodiscard]] std::optional<std::filesystem::path> getConfigFile(const std::string& name);
