#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../input/hotkey.hpp"
#include "ast.hpp"
#include "interpreter.hpp"
#include "parser.hpp"

struct ConfigLoadResult {
    ast::Program program;
    std::vector<Binding> bindings;
    std::vector<TapBinding> tapBindings;
    ConfigProperties config;
    std::vector<ParseError> parseErrors;
    std::vector<InterpreterError> interpreterErrors;
    std::optional<std::string> fileError;
};

class ConfigLoader {
   public:
    static ConfigLoadResult loadFromContents(std::string_view contents);
    static ConfigLoadResult loadFromFile(const std::filesystem::path& path);
};
