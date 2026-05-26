/**
 * @file world_loader.hpp
 * @brief Deserialisation of a complete @ref World from on-disk data files.
 *
 * @details The world loader reads JSON files from a scenario
 * data directory and assembles them into a fully initialised @ref World:
 *
 * - @c world.json — location graph, item registry, and player start location.
 * - @c npcs.json  — NPC identity and initial state for every character.
 * - @c facts.json — authored knowledge registry.
 * - @c flags.json — authored narrative flag declarations.
 * - @c events.json — scripted event triggers.
 *
 * Entity IDs are injected from each JSON object's map key so the data files
 * do not need to repeat the key inside the object body.  NPCs are cross-
 * referenced into their starting locations automatically.
 */

#pragma once
#include "entities/world.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace chronicle {

/// @brief Explicit set of world data files in a scenario package.
///
/// @details Paths are resolved and safety-checked by the scenario package
/// layer before they reach @ref load_world.  This struct intentionally excludes
/// @c config.json, which is loaded separately by @ref Config.
struct WorldFileSet {
    /// Location graph, item registry, and player start location.
    std::filesystem::path world;

    /// NPC identity, initial state, and tool policies.
    std::filesystem::path npcs;

    /// Authored fact registry.
    std::filesystem::path facts;

    /// Declared narrative flag defaults.
    std::filesystem::path flags;

    /// Deterministic scripted event triggers.
    std::filesystem::path events;
};

/// @brief Load a complete @ref World from explicit scenario data files.
///
/// @details Loads files in deterministic order: world, flags, facts, NPCs,
/// and events.  Entity IDs are injected from JSON map keys, NPCs are inserted
/// into their starting locations, the clock is default-constructed, and
/// @ref validate_world runs before the result is returned.
///
/// @param files Resolved data-file paths for a scenario package.
/// @param warnings_out Optional output; validation warnings are appended when set.
/// @return A fully populated and structurally validated world.
/// @throws std::runtime_error if any file cannot be opened, contains malformed
///         JSON, omits required fields, or fails world validation.
World load_world(const WorldFileSet &files, std::vector<std::string> *warnings_out = nullptr);

} // namespace chronicle
