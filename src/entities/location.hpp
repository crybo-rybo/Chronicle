/**
 * @file location.hpp
 * @brief Location entity definition for Chronicle.
 *
 * @details Locations form the navigable graph of the game world.  Each
 * location is a named, describable place connected to its neighbours by named
 * exits.  The location also serves as the authoritative container for the item
 * IDs and NPC IDs that are currently present there.
 */

#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace chronicle {

struct World; // forward declaration — defined in world.hpp

/// @brief A single navigable place in the game world.
///
/// @details Locations make up the spatial graph of the game.  They are
/// connected by named exits (e.g. @c "north" → another location ID) and
/// act as containers for the items and NPCs currently present.
///
/// The description system supports a two-level fallback: a @c current_description
/// that can be changed at runtime by scripted events overrides the static
/// @c base_description.
struct Location {
    /// Unique identifier, set from the JSON map key during world load.
    std::string id;

    /// Human-readable display name, e.g. @c "The Tavern".
    std::string name;

    /// Permanent description text loaded from the world data file.
    std::string base_description;

    /// @brief Optional runtime-override description.
    ///
    /// When non-empty, @ref describe returns this value instead of
    /// @c base_description.  Scripted events can write here to reflect
    /// significant world-state changes (e.g. after a fire breaks out).
    std::string current_description;

    /// @brief Named exits to adjacent locations.
    ///
    /// Maps a direction string (e.g. @c "north", @c "upstairs") to the ID
    /// of the destination location.
    std::map<std::string, std::string> exits;

    /// IDs of @ref Item objects currently present in this location.
    std::vector<std::string> items;

    /// IDs of @ref Npc objects currently present in this location.
    std::vector<std::string> npcs;

    /// @brief Exit directions that are currently locked.
    ///
    /// A locked exit is listed in @c exits but cannot be traversed until
    /// the corresponding unlock condition is met (e.g. a key item is used).
    std::vector<std::string> locked_exits;

    /// @brief Returns the appropriate description string for display.
    ///
    /// @details Returns @c current_description if it is non-empty; otherwise
    /// falls back to @c base_description.  The @p world parameter is accepted
    /// for future use (e.g. incorporating NPC/item state into the description)
    /// but is currently unused.
    ///
    /// @param world The current world state (reserved for future use).
    /// @return The effective description string.
    std::string describe(const World &world) const;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const Location &loc);
void from_json(const nlohmann::json &j, Location &loc);
/// @endcond

} // namespace chronicle
