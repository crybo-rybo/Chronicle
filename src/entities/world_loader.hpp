/**
 * @file world_loader.hpp
 * @brief Deserialisation of a complete @ref World from on-disk data files.
 *
 * @details The world loader reads three separate JSON files from a scenario
 * data directory and assembles them into a fully initialised @ref World:
 *
 * - @c world.json — location graph, item registry, and player start location.
 * - @c npcs.json  — NPC identity and initial state for every character.
 * - @c events.json — scripted event triggers.
 *
 * Entity IDs are injected from each JSON object's map key so the data files
 * do not need to repeat the key inside the object body.  NPCs are cross-
 * referenced into their starting locations automatically.
 */

#pragma once
#include "entities/world.hpp"
#include <filesystem>

namespace chronicle {

/// @brief Load a complete @ref World from the scenario data directory.
///
/// @details Reads and parses @c world.json, @c npcs.json, and @c events.json
/// from @p data_dir.  Item IDs and location IDs are injected from JSON map
/// keys.  Each NPC's ID is also normalised to match its map key, and the NPC
/// is registered in the appropriate @ref Location::npcs list.  The
/// @ref Clock is default-constructed to Day 1, Morning.
///
/// @param data_dir Directory containing the three required data files.
/// @return A fully initialised @ref World ready for gameplay.
/// @throws std::runtime_error if any required file is missing, cannot be
///         opened, or contains invalid JSON.
World load_world(const std::filesystem::path &data_dir);

} // namespace chronicle
