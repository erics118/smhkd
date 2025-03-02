#include <fstream>

#include "cli.hpp"
#include "locale.hpp"
#include "log.hpp"
#include "parser.hpp"
#include "service.hpp"
#include "utils.hpp"

int main(int argc, char* argv[]) {
    ArgsConfig config{
        .short_args = {"c:"},
        .long_args = {"config:"},
    };
    Args args = parse_args(std::vector<std::string>(argv, argv + argc), config);
    std::string config_file = args.get('c', "config").value_or(get_config_file("smhkd").value_or(""));

    if (config_file.empty() || !file_exists(config_file)) {
        error("config file not found");
        return 1;
    }

    info("config file set to: {}", config_file);

    std::ifstream file(config_file);

    // Read entire file into string
    std::string configFileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    try {
        initializeKeycodeMap();

        Tokenizer tokenizer(configFileContents);

        Parser parser(tokenizer);

        std::vector<Hotkey> hotkeys = parser.parseFile();

        for (const auto& hk : hotkeys) {
            debug("Hotkey: {}", hk);
        }

        auto service = std::make_unique<Service>(hotkeys);

        service->init();

        service->run();

    } catch (const std::exception& ex) {
        error("Error while parsing hotkeys: {}\n", ex.what());
    }
}
