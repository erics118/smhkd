#pragma once

#include <map>
#include <string>

#include "ast.hpp"
#include "config_properties.hpp"
#include "hotkey.hpp"

struct InterpreterResult {
    std::map<Hotkey, std::string> hotkeys;
    ConfigProperties config;
};

class Interpreter {
   public:
    Interpreter() = default;

    [[nodiscard]] InterpreterResult interpret(const Program& program);
};
