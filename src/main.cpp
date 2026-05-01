/**
 * @file main.cpp
 * @brief Entry point for the Chronicle scenario runtime.
 *
 * @details Bootstraps the runtime and launches the @ref GameEngine.
 * Scenario packages are loaded from @c scenario.json manifests.  If the
 * manifest, config, or world data is missing or malformed, the error is
 * reported and the process exits with a non-zero status.
 */

#include "diagnostics/logger.hpp"
#include "engine/cli.hpp"
#include "engine/game_engine.hpp"
#include "entities/scenario.hpp"
#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    chronicle::logging::configure_from_environment();
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        auto parsed = chronicle::parse_cli_args(args);
        if (auto *error = std::get_if<std::string>(&parsed)) {
            std::cerr << *error;
            return 2;
        }

        const auto &options = std::get<chronicle::CliOptions>(parsed);
        if (options.mode == chronicle::CliMode::Help) {
            std::cout << chronicle::cli_usage();
            return 0;
        }

        if (options.mode == chronicle::CliMode::Validate) {
            auto report = chronicle::validate_scenario_package(options.scenario_dir);
            for (const auto &warning : report.warnings) {
                std::cerr << "Warning: " << warning << "\n";
            }
            if (!report.ok) {
                for (const auto &error : report.errors) {
                    std::cerr << "Error: " << error << "\n";
                }
                return 1;
            }
            std::cout << "Scenario package is valid: " << options.scenario_dir << "\n";
            return 0;
        }

        auto package = chronicle::load_scenario_package(options.scenario_dir);
        chronicle::GameEngine engine(package.config_path.string(), package.world_files, nullptr);
        engine.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
