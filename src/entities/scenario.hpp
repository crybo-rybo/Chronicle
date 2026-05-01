/**
 * @file scenario.hpp
 * @brief Scenario package manifest loading and validation.
 */

#pragma once

#include "entities/world_loader.hpp"
#include "entities/world_validator.hpp"
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

inline constexpr int kCurrentScenarioSchemaVersion = 1;

/// @brief Relative file paths declared by a scenario package manifest.
struct ScenarioFileManifest {
    std::string config = "config.json";
    std::string world = "world.json";
    std::string npcs = "npcs.json";
    std::string facts = "facts.json";
    std::string flags = "flags.json";
    std::string events = "events.json";
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScenarioFileManifest, config, world, npcs, facts,
                                                flags, events)
/// @endcond

/// @brief Author-facing scenario manifest loaded from @c scenario.json.
struct ScenarioManifest {
    std::string id;
    std::string name;
    std::string version;
    int chronicle_schema_version = kCurrentScenarioSchemaVersion;
    ScenarioFileManifest files;
    std::map<std::string, std::string> metadata;
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScenarioManifest, id, name, version,
                                                chronicle_schema_version, files, metadata)
/// @endcond

/// @brief Fully resolved scenario package paths ready for runtime loading.
struct ScenarioPackage {
    std::filesystem::path root_dir;
    ScenarioManifest manifest;
    std::filesystem::path config_path;
    WorldFileSet world_files;
};

/// @brief Parse @c scenario.json from a scenario package directory.
ScenarioManifest load_scenario_manifest(const std::filesystem::path &scenario_dir);

/// @brief Parse a package manifest and resolve all referenced file paths.
ScenarioPackage load_scenario_package(const std::filesystem::path &scenario_dir);

/// @brief Validate a package manifest, referenced files, and world content.
ValidationReport validate_scenario_package(const std::filesystem::path &scenario_dir);

} // namespace chronicle
