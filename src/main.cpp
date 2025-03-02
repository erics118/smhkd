#include "cli.hpp"
#include "log.hpp"
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
}
