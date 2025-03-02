#include <fstream>

#include "cli.hpp"
#include "log.hpp"
#include "parser.hpp"
#include "utils.hpp"

int main(int argc, char* argv[]) {
    ArgsConfig config{
        .short_args = {"c:"},
        .long_args = {"config:"},
    };
    Args args = parse_args(std::vector<std::string>(argv, argv + argc), config);
    std::string config_file = args.get_opt('c', "config").value_or(get_config_file("smhkd").value_or(""));

    if (config_file.empty() || !file_exists(config_file)) {
        error("config file not found");
        return 1;
    }

    info("config file set to: {}", config_file);

    std::ifstream file(config_file);

    // Read entire file into string
    std::string configFileContents((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    try {
        // 1) Initialize tokenizer with file contents
        Tokenizer tokenizer(configFileContents);

        // 2) Parse hotkeys
        Parser parser(tokenizer);
        std::vector<Hotkey> hotkeys = parser.parseFile();

        // 3) Print out the results
        for (const auto& hk : hotkeys) {
            debug("Hotkey: {}", hk);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error while parsing hotkeys: " << ex.what() << std::endl;
    }
}
