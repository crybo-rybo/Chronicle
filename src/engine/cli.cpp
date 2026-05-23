/**
 * @file cli.cpp
 * @brief Implementation of command-line parsing for the Chronicle scenario runtime.
 */

#include "engine/cli.hpp"

namespace chronicle {

std::string cli_usage() {
    return "Usage:\n"
           "  chronicle [--scenario <dir>]\n"
           "  chronicle inspect --scenario <dir>\n"
           "  chronicle validate --scenario <dir>\n";
}

std::variant<CliOptions, std::string> parse_cli_args(const std::vector<std::string> &args) {
    CliOptions options;
    std::size_t index = 0;

    if (index < args.size() && args[index] == "validate") {
        options.mode = CliMode::Validate;
        ++index;
    } else if (index < args.size() && args[index] == "inspect") {
        options.mode = CliMode::Inspect;
        ++index;
    } else if (index < args.size() && (args[index] == "--help" || args[index] == "-h")) {
        options.mode = CliMode::Help;
        return options;
    }

    while (index < args.size()) {
        const auto &arg = args[index];
        if (arg == "--scenario") {
            if (index + 1 >= args.size()) {
                return std::string("--scenario requires a directory argument\n") + cli_usage();
            }
            options.scenario_dir = args[index + 1];
            index += 2;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            options.mode = CliMode::Help;
            ++index;
            continue;
        }
        return "Unknown argument: " + arg + "\n" + cli_usage();
    }

    return options;
}

} // namespace chronicle
