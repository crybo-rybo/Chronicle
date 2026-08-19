// Chronicle CLI: argument parsing and subcommands.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "chronicle/cartridge/models.hpp"

namespace chronicle {

// Parsed command line. Exposed for unit tests; run_cli() is the entry point.
struct CliArgs {
    std::string command = "play"; // play|run|validate|inspect|list|install|pack
    std::optional<std::string> scenario;
    std::optional<std::string> base_url;
    std::optional<std::string> model;
    std::optional<std::string> output;
    std::vector<std::string> positional; // run <id>, install <path>
    bool tiny = false;
    bool version = false;
    bool help = false;
    std::optional<std::string> error; // set when parsing failed
};

[[nodiscard]] CliArgs parse_cli(const std::vector<std::string> &args);

// Build the world used by `chronicle --tiny`: one room, one say-only NPC.
[[nodiscard]] WorldState build_tiny_world();

int run_cli(int argc, char **argv);

} // namespace chronicle
