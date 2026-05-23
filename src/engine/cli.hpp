/**
 * @file cli.hpp
 * @brief Command-line parsing for the Chronicle scenario runtime.
 *
 * @details The CLI is part of Chronicle's v1 public contract.  Supported forms
 * are:
 * @code
 * chronicle [--scenario <dir>]
 * chronicle inspect --scenario <dir>
 * chronicle validate --scenario <dir>
 * chronicle --help
 * @endcode
 * If no scenario is provided, the bundled @c data package is used.
 */

#pragma once

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace chronicle {

/// @brief Top-level process mode selected by command-line arguments.
enum class CliMode {
    Run,      ///< Load the scenario and start the interactive game loop.
    Inspect,  ///< Print scenario package identity and validation readiness.
    Validate, ///< Validate the scenario package and exit without starting play.
    Help      ///< Print usage text and exit successfully.
};

/// @brief Parsed command-line options used by @c main.cpp.
struct CliOptions {
    /// Requested process mode.  Defaults to @ref CliMode::Run.
    CliMode mode = CliMode::Run;

    /// Scenario package directory.  Defaults to bundled sample package @c data.
    std::filesystem::path scenario_dir = "data";
};

/// @brief Parse process arguments into strongly typed CLI options.
///
/// @details Expects @p args to exclude the executable name.  Returns
/// @ref CliOptions on success or a ready-to-print error string that includes
/// usage text on failure.  Parsing is intentionally model-free and does not
/// check whether the scenario path exists.
///
/// @param args Command-line tokens after @c argv[0].
/// @return Parsed options, or an error/usage string.
std::variant<CliOptions, std::string> parse_cli_args(const std::vector<std::string> &args);

/// @brief Return the stable command-line usage text.
std::string cli_usage();

} // namespace chronicle
