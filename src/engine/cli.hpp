/**
 * @file cli.hpp
 * @brief Command-line parsing for the Chronicle scenario runtime.
 *
 * @details The CLI is part of Chronicle's v1 public contract.  Supported forms
 * include cartridge library management (@c list, @c run, @c install, @c pack)
 * and creator validation tools (@c inspect, @c validate).
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace chronicle {

/// @brief Top-level process mode selected by command-line arguments.
enum class CliMode {
    Run,      ///< Load the scenario and start the interactive game loop.
    Inspect,  ///< Print scenario package identity and validation readiness.
    Validate, ///< Validate the scenario package and exit without starting play.
    List,     ///< List cartridges discovered in library roots.
    Install,  ///< Install a cartridge archive or directory into the library.
    Pack,     ///< Validate and pack a cartridge directory into a .chronicle archive.
    Help      ///< Print usage text and exit successfully.
};

/// @brief Parsed command-line options used by @c main.cpp.
struct CliOptions {
    CliMode mode = CliMode::Run;

    /// Scenario package directory or manifest ID for run/inspect/validate/pack.
    std::filesystem::path scenario_dir = "data";

    /// Cartridge ID for @ref CliMode::Run when launching from the library.
    std::optional<std::string> cartridge_id;

    /// Source archive or directory for @ref CliMode::Install.
    std::filesystem::path install_source;

    /// Output archive path for @ref CliMode::Pack.
    std::filesystem::path pack_output;

    /// Optional library root override for install/list/run resolution.
    std::optional<std::filesystem::path> library_root;
};

std::variant<CliOptions, std::string> parse_cli_args(const std::vector<std::string> &args);
std::string cli_usage();

} // namespace chronicle
