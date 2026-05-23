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
#include "entities/config.hpp"
#include "entities/scenario.hpp"
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void print_metadata_field(const chronicle::ScenarioManifest &manifest, const std::string &key,
                          const std::string &label, bool &printed_any) {
    auto it = manifest.metadata.find(key);
    if (it == manifest.metadata.end()) {
        return;
    }
    std::cout << "  " << label << ": " << it->second << "\n";
    printed_any = true;
}

void print_scenario_inspection(const chronicle::ScenarioPackage &package,
                               const chronicle::ValidationReport &report) {
    const auto &manifest = package.manifest;

    std::cout << "Chronicle cartridge\n";
    std::cout << "  Name: " << manifest.name << "\n";
    std::cout << "  ID: " << manifest.id << "\n";
    std::cout << "  Version: " << manifest.version << "\n";
    std::cout << "  Schema: " << manifest.chronicle_schema_version << "\n";
    std::cout << "  Root: " << package.root_dir.string() << "\n\n";

    std::cout << "Metadata\n";
    bool printed_metadata = false;
    print_metadata_field(manifest, "description", "Description", printed_metadata);
    print_metadata_field(manifest, "author", "Author", printed_metadata);
    print_metadata_field(manifest, "license", "License", printed_metadata);
    if (!printed_metadata) {
        std::cout << "  (none)\n";
    }
    std::cout << "\n";

    std::cout << "Files\n";
    std::cout << "  Config: " << manifest.files.config << "\n";
    std::cout << "  World: " << manifest.files.world << "\n";
    std::cout << "  NPCs: " << manifest.files.npcs << "\n";
    std::cout << "  Facts: " << manifest.files.facts << "\n";
    std::cout << "  Flags: " << manifest.files.flags << "\n";
    std::cout << "  Events: " << manifest.files.events << "\n\n";

    std::cout << "Readiness\n";
    std::cout << "  Status: " << (report.ok ? "valid" : "invalid") << "\n";
    for (const auto &warning : report.warnings) {
        std::cout << "  Warning: " << warning << "\n";
    }
    for (const auto &error : report.errors) {
        std::cout << "  Error: " << error << "\n";
    }
}

} // namespace

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

        if (options.mode == chronicle::CliMode::Inspect) {
            try {
                auto package = chronicle::load_scenario_package(options.scenario_dir);
                auto report = chronicle::validate_scenario_package(options.scenario_dir);
                print_scenario_inspection(package, report);
                return report.ok ? 0 : 1;
            } catch (const std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                return 1;
            }
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
        auto config = chronicle::Config::load_with_operator_overrides(package.config_path);
        chronicle::GameEngine engine(std::move(config), package.config_path.string(),
                                     package.world_files, nullptr);
        engine.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
