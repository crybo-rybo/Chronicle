/**
 * @file cartridge_archive.hpp
 * @brief Pack and install Chronicle cartridge archives.
 *
 * @details A @c .chronicle archive is a gzip-compressed tar file whose root
 * contains @c scenario.json and the six package data files.  The format keeps
 * cartridges data-only and mirrors the directory layout used by
 * @c chronicle --scenario <dir>.
 */

#pragma once

#include "entities/scenario.hpp"
#include <filesystem>
#include <string>

namespace chronicle {

/// @brief Create a @c .chronicle archive from a validated scenario directory.
///
/// @details Runs @c chronicle validate semantics first.  Writes @p output_path,
/// creating parent directories as needed.
///
/// @return The archive path that was written after default suffix resolution.
/// @throws std::runtime_error if validation fails or the archive cannot be created.
std::filesystem::path pack_cartridge(const std::filesystem::path &scenario_dir,
                                     const std::filesystem::path &output_path);

/// @brief Install a @c .chronicle archive or directory into a library root.
///
/// @details Extracts archives into @c library_root/<manifest-id>/ and returns
/// the installed package directory.  Directories are copied when @p source is
/// not an archive file.
///
/// @throws std::runtime_error if validation or installation fails.
ScenarioPackage install_cartridge(const std::filesystem::path &source,
                                  const std::filesystem::path &library_root);

} // namespace chronicle
