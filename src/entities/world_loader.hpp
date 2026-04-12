#pragma once
#include "entities/world.hpp"
#include <filesystem>

namespace chronicle {

/// Load a complete World from data files in the given directory.
/// Expects: world.json, npcs.json, events.json
/// Throws std::runtime_error on missing files or parse errors.
World load_world(const std::filesystem::path &data_dir);

} // namespace chronicle
