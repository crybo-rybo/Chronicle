// Load and path-confine scenario packages.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "chronicle/cartridge/models.hpp"

namespace chronicle {

// Raised when a package cannot be loaded.
class CartridgeError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

inline constexpr std::uintmax_t MAX_PACKAGE_FILE_BYTES = 4U * 1024U * 1024U;
inline constexpr std::uintmax_t MAX_PACKAGE_TOTAL_BYTES = 32U * 1024U * 1024U;
inline constexpr std::size_t MAX_PACKAGE_FILES = 256;

struct PackageContents {
    std::vector<std::filesystem::path> files;
    std::uintmax_t total_bytes = 0;
};

// Inspect every entry in an untrusted package. Symlinks, special files, and
// packages exceeding the fixed resource budget are rejected.
[[nodiscard]] PackageContents inspect_package_tree(const std::filesystem::path &package_dir);

[[nodiscard]] ScenarioManifest load_manifest(const std::filesystem::path &package_dir);

// Assemble a runtime WorldState from a package directory. Throws CartridgeError.
[[nodiscard]] WorldState load_package(const std::filesystem::path &package_dir);

// Assemble a WorldState from already-parsed JSON documents (used by --tiny and tests).
[[nodiscard]] WorldState
assemble_world(const nlohmann::json &manifest_raw, const nlohmann::json &config_raw,
               const nlohmann::json &world_raw, const nlohmann::json &npcs_raw,
               const nlohmann::json &facts_raw, const nlohmann::json &flags_raw,
               const nlohmann::json &events_raw);

} // namespace chronicle
