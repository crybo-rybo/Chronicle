// Cartridge library: list / install / pack / resolve.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "chronicle/cartridge/validator.hpp"

namespace chronicle {

struct CartridgeInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string path;
};

struct InspectInfo {
    std::string id;
    std::string name;
    std::string version;
    int schema = 0;
    std::string path;
    bool ready = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class LibraryError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] std::filesystem::path default_library_dir();

[[nodiscard]] std::vector<CartridgeInfo>
list_cartridges(const std::optional<std::filesystem::path> &library_dir = std::nullopt);

// Install a scenario directory or .chronicle/.zip archive; returns the
// installed path. Throws LibraryError on validation or archive failures.
std::filesystem::path
install_cartridge(const std::filesystem::path &source,
                  const std::optional<std::filesystem::path> &library_dir = std::nullopt);

// Pack a validated scenario directory into a zip archive.
std::filesystem::path pack_cartridge(const std::filesystem::path &package_dir,
                                     const std::filesystem::path &output);

// Resolve a --scenario argument: directory path, installed library id, or the
// bundled examples/minimal fallback. Throws LibraryError when nothing matches.
[[nodiscard]] std::filesystem::path
resolve_scenario(const std::optional<std::string> &scenario,
                 const std::optional<std::filesystem::path> &library_dir = std::nullopt);

[[nodiscard]] InspectInfo inspect_package(const std::filesystem::path &package_dir);

} // namespace chronicle
