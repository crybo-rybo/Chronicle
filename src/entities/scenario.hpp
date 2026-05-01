/**
 * @file scenario.hpp
 * @brief Scenario package manifest loading and validation.
 *
 * @details The scenario layer is part of Chronicle's v1 creator-facing
 * contract.  It owns the @c scenario.json manifest format, resolves package
 * file paths, and provides the model-free validation entry point used by
 * @c chronicle validate --scenario <dir>.
 */

#pragma once

#include "entities/world_loader.hpp"
#include "entities/world_validator.hpp"
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

/// @brief Scenario package schema version supported by this runtime.
///
/// @details @ref load_scenario_manifest rejects manifests whose
/// @c chronicle_schema_version differs from this value.  Scenario schema
/// versioning is separate from save-file schema versioning.
inline constexpr int kCurrentScenarioSchemaVersion = 1;

/// @brief Relative file paths declared by a scenario package manifest.
///
/// @details Each entry defaults to the canonical v1 file name, but
/// @c scenario.json may override it with a different relative path.  Loaders
/// reject absolute paths and any path that would resolve outside the package
/// root.
struct ScenarioFileManifest {
    /// Runtime configuration file, usually @c config.json.
    std::string config = "config.json";

    /// Location graph, item registry, and player start file.
    std::string world = "world.json";

    /// NPC identity, initial state, and tool policy file.
    std::string npcs = "npcs.json";

    /// Authored fact registry file.
    std::string facts = "facts.json";

    /// Declared narrative flag defaults file.
    std::string flags = "flags.json";

    /// Deterministic event trigger file.
    std::string events = "events.json";
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScenarioFileManifest, config, world, npcs, facts,
                                                flags, events)
/// @endcond

/// @brief Author-facing scenario manifest loaded from @c scenario.json.
///
/// @details The manifest identifies the package and points to the six required
/// v1 data files.  @c metadata is intentionally limited to string-to-string
/// values so authoring tools can attach simple descriptive fields without
/// affecting runtime behavior.
struct ScenarioManifest {
    /// Stable scenario package ID, chosen by the package author.
    std::string id;

    /// Human-readable scenario name.
    std::string name;

    /// Scenario content version, independent of Chronicle's schema version.
    std::string version;

    /// Must match @ref kCurrentScenarioSchemaVersion.
    int chronicle_schema_version = kCurrentScenarioSchemaVersion;

    /// Relative paths to all package data files.
    ScenarioFileManifest files;

    /// Optional string metadata such as description, author, or license.
    std::map<std::string, std::string> metadata;
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScenarioManifest, id, name, version,
                                                chronicle_schema_version, files, metadata)
/// @endcond

/// @brief Fully resolved scenario package paths ready for runtime loading.
///
/// @details Returned by @ref load_scenario_package after the manifest has been
/// parsed and all file paths have been normalized under @c root_dir.  The
/// object is consumed by process startup and @ref GameEngine construction.
struct ScenarioPackage {
    /// Absolute, normalized package root directory.
    std::filesystem::path root_dir;

    /// Parsed manifest contents.
    ScenarioManifest manifest;

    /// Resolved path to @ref Config's JSON file.
    std::filesystem::path config_path;

    /// Resolved paths to the world data files consumed by @ref load_world.
    WorldFileSet world_files;
};

/// @brief Parse @c scenario.json from a scenario package directory.
///
/// @param scenario_dir Package root containing @c scenario.json.
/// @return Parsed manifest with defaulted file names applied.
/// @throws std::runtime_error if the manifest cannot be opened, parsed, or
///         uses an unsupported @c chronicle_schema_version.
ScenarioManifest load_scenario_manifest(const std::filesystem::path &scenario_dir);

/// @brief Parse a package manifest and resolve all referenced file paths.
///
/// @details Path resolution is defensive: declared paths must be relative and
/// must remain inside the package root after lexical normalization.  This
/// function does not load the world files or require a model.
///
/// @param scenario_dir Package root containing @c scenario.json.
/// @return A package object with absolute, normalized file paths.
/// @throws std::runtime_error if manifest loading fails or any referenced path
///         is absolute or escapes the package root.
ScenarioPackage load_scenario_package(const std::filesystem::path &scenario_dir);

/// @brief Validate a package manifest, referenced files, and world content.
///
/// @details Creator-facing validation entry point.  It checks that the
/// directory and all manifest-declared files exist, then calls @ref load_world
/// so JSON parsing and cross-reference validation run without constructing
/// any model-backed AI objects.
///
/// @param scenario_dir Package root to validate.
/// @return A report whose errors block execution and whose warnings, when
///         populated, are authoring guidance.
ValidationReport validate_scenario_package(const std::filesystem::path &scenario_dir);

} // namespace chronicle
