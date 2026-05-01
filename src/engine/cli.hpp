/**
 * @file cli.hpp
 * @brief Command-line parsing for the Chronicle scenario runtime.
 */

#pragma once

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace chronicle {

enum class CliMode { Run, Validate, Help };

struct CliOptions {
    CliMode mode = CliMode::Run;
    std::filesystem::path scenario_dir = "data";
};

std::variant<CliOptions, std::string> parse_cli_args(const std::vector<std::string> &args);

std::string cli_usage();

} // namespace chronicle
