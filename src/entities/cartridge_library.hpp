/**
 * @file cartridge_library.hpp
 * @brief Discover and resolve installed scenario cartridges for the console library.
 *
 * @details Chronicle scans configurable library roots for directories containing
 * @c scenario.json manifests.  Each discovered package is indexed by manifest
 * @c id so players can launch titles with @c chronicle run <id>.
 */

#pragma once

#include "entities/scenario.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace chronicle {

/// @brief Summary of one cartridge discovered in a library root.
struct CartridgeEntry {
    std::string id;                         ///< Manifest @c id.
    std::string name;                       ///< Manifest @c name.
    std::string version;                    ///< Manifest content @c version.
    int chronicle_schema_version = 1;       ///< Manifest schema version.
    std::filesystem::path root_dir;         ///< Absolute package directory.
    std::map<std::string, std::string> metadata; ///< Optional manifest metadata.
};

/// @brief Resolve the ordered list of cartridge library roots to scan.
///
/// @details Precedence (first match wins for duplicate IDs):
/// 1. Paths from @c CHRONICLE_LIBRARY_PATH (platform path separator)
/// 2. @c ~/.chronicle/cartridges
/// 3. @c ./cartridges relative to the current working directory
///
/// @return Existing directories in scan order.
std::vector<std::filesystem::path> default_cartridge_library_roots();

/// @brief Scan library roots and return discovered cartridges.
///
/// @details Invalid manifests are skipped silently.  When the same @c id appears
/// in multiple roots, the first root in @ref default_cartridge_library_roots wins.
///
/// @param extra_roots Additional roots appended after the defaults.
std::vector<CartridgeEntry> list_cartridges(
    const std::vector<std::filesystem::path> &extra_roots = {});

/// @brief Find a cartridge by manifest @c id.
///
/// @param cartridge_id Stable package ID from @c scenario.json.
/// @param extra_roots Additional library roots to scan.
/// @return The resolved entry, or @c std::nullopt if not found.
std::optional<CartridgeEntry> find_cartridge(
    std::string_view cartridge_id,
    const std::vector<std::filesystem::path> &extra_roots = {});

/// @brief Resolve a cartridge path from an explicit directory or library ID.
///
/// @details If @p scenario_ref is an existing filesystem path, it is returned
/// directly.  Otherwise it is treated as a manifest @c id lookup.
std::optional<std::filesystem::path>
resolve_cartridge_path(std::string_view scenario_ref,
                       const std::vector<std::filesystem::path> &extra_roots = {});

} // namespace chronicle
