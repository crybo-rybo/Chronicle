// Load and path-confine scenario packages.
#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "chronicle/cartridge/models.hpp"

namespace chronicle {

// Raised when a package cannot be loaded.
class CartridgeError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

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
