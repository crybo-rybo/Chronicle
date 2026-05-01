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

namespace chronicle {

/// @brief Explicit set of world data files in a scenario package.
struct WorldFileSet {
    std::filesystem::path world;
    std::filesystem::path npcs;
    std::filesystem::path facts;
    std::filesystem::path flags;
    std::filesystem::path events;
};

/// @brief Load a complete @ref World from explicit scenario data files.
World load_world(const WorldFileSet &files);

} // namespace chronicle
