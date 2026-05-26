/**
 * @file cli.cpp
 * @brief Implementation of command-line parsing for the Chronicle scenario runtime.
 */

#include "engine/cli.hpp"
#include <string_view>

namespace chronicle {

std::string cli_usage() {
    return "Usage:\n"
           "  chronicle [--scenario <dir>]\n"
           "  chronicle run <cartridge-id>\n"
           "  chronicle list\n"
           "  chronicle install <path.chronicle|dir> [--library <dir>]\n"
           "  chronicle pack --scenario <dir> [--output <path.chronicle>]\n"
           "  chronicle inspect --scenario <dir>\n"
           "  chronicle validate --scenario <dir>\n";
}

std::variant<CliOptions, std::string> parse_cli_args(const std::vector<std::string> &args) {
    CliOptions options;
    std::size_t index = 0;

    if (index < args.size()) {
        const auto &head = args[index];
        if (head == "validate") {
            options.mode = CliMode::Validate;
            ++index;
        } else if (head == "inspect") {
            options.mode = CliMode::Inspect;
            ++index;
        } else if (head == "list") {
            options.mode = CliMode::List;
            ++index;
        } else if (head == "run") {
            options.mode = CliMode::Run;
            ++index;
            if (index >= args.size()) {
                return std::string("run requires a cartridge id\n") + cli_usage();
            }
            options.cartridge_id = args[index++];
        } else if (head == "install") {
            options.mode = CliMode::Install;
            ++index;
            if (index >= args.size()) {
                return std::string("install requires a cartridge archive or directory\n") +
                       cli_usage();
            }
            options.install_source = args[index++];
        } else if (head == "pack") {
            options.mode = CliMode::Pack;
            ++index;
        } else if (head == "--help" || head == "-h") {
            options.mode = CliMode::Help;
            return options;
        }
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
        if (arg == "--library") {
            if (index + 1 >= args.size()) {
                return std::string("--library requires a directory argument\n") + cli_usage();
            }
            options.library_root = args[index + 1];
            index += 2;
            continue;
        }
        if (arg == "--output") {
            if (index + 1 >= args.size()) {
                return std::string("--output requires a path argument\n") + cli_usage();
            }
            options.pack_output = args[index + 1];
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

    if (options.mode == CliMode::Pack && options.scenario_dir == "data") {
        bool has_scenario_flag = false;
        for (const auto &arg : args) {
            if (arg == "--scenario") {
                has_scenario_flag = true;
                break;
            }
        }
        if (!has_scenario_flag) {
            return std::string("pack requires --scenario <dir>\n") + cli_usage();
        }
    }

    return options;
}

} // namespace chronicle
