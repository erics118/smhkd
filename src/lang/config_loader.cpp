#include "config_loader.hpp"

#include <fstream>
#include <iterator>

ConfigLoadResult ConfigLoader::loadFromContents(std::string_view contents) {
    ConfigLoadResult result{};
    const std::string source(contents);

    Parser parser(source);
    result.program = parser.parseProgram();
    result.parseErrors = parser.errors();

    Interpreter interpreter;
    auto interpreterResult = interpreter.interpret(result.program);
    result.hotkeys = std::move(interpreterResult.hotkeys);
    result.remaps = std::move(interpreterResult.remaps);
    result.config = interpreterResult.config;
    result.interpreterErrors = std::move(interpreterResult.errors);

    return result;
}

ConfigLoadResult ConfigLoader::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        ConfigLoadResult result{};
        result.fileError = std::format("failed to open config file '{}'", path);
        return result;
    }

    const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto result = loadFromContents(contents);
    if (!file.good() && !file.eof()) {
        result.fileError = std::format("failed to read config file '{}'", path);
    }
    return result;
}
