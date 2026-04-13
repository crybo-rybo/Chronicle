/**
 * @file item.hpp
 * @brief Item entity definition for Chronicle.
 *
 * @details Items are the physical objects of the game world: keys, notes,
 * weapons, trinkets, and anything else the player or an NPC can possess.
 * The canonical @ref Item record lives in @ref World::items; every other
 * container (player inventory, NPC inventory, location item list) holds only
 * the item's string ID — never a copy of the struct itself.
 */

#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

/// @brief A game item.
///
/// @details Items have a single canonical entry in @ref World::items; ownership
/// is tracked by item ID in player, NPC, and location containers.  Items never
/// move from the registry — only their associated ownership IDs change.
///
/// The @c id field is injected from the JSON map key during initial world load
/// and is also serialised directly into save/load JSON so that the registry can
/// reconstruct itself without depending on surrounding key context.
struct Item {
    /// @brief Unique identifier for this item.
    ///
    /// @details Set from the JSON map key during initial world load and
    /// also serialised in to_json/from_json for save/load round-trips.
    /// The world loader is the canonical source of truth for this value.
    std::string id;

    /// Human-readable display name, e.g. @c "Rusty Key".
    std::string name;

    /// Flavour text shown when the player examines this item.
    std::string description;

    /// Whether the player may pick this item up from a location.
    bool takeable = true;

    /// @brief Prevents the item from being dropped or traded when @c true.
    ///
    /// Plot-critical items (e.g. the only key to the final door) are marked
    /// as key items so the LLM cannot accidentally remove them from the game.
    bool key_item = false;

    /// @brief Hides the item from the room description when @c true.
    ///
    /// Hidden items must be found through explicit examination or scripted
    /// reveals; they do not appear in the ambient "You see: …" listing.
    bool hidden = false;

    /// @brief ID of the location exit unlocked by this item, if any.
    ///
    /// An empty string means the item has no unlock effect.  When non-empty,
    /// the engine checks this field during @c use item on door logic.
    std::string unlock_target;

    /// @brief Extensible key/value metadata.
    ///
    /// Arbitrary properties used by game logic without requiring struct changes,
    /// e.g. @c {"readable": "true", "text": "Help me…"}.
    std::map<std::string, std::string> properties;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const Item &item);
void from_json(const nlohmann::json &j, Item &item);
/// @endcond

} // namespace chronicle
