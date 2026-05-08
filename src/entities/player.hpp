/**
 * @file player.hpp
 * @brief Player entity definition for Chronicle.
 *
 * @details The Player struct holds all mutable state that belongs to the
 * human-controlled character: current location, carried items, discovered
 * facts, and per-NPC encounter history.  Like all world state, it is owned
 * exclusively by @ref World and mutated only through the @ref GameEngine
 * mutation pipeline.
 */

#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace chronicle {

/// @brief Mutable state representing the player character.
///
/// @details The player has no fixed identity fields analogous to
/// @ref NpcIdentity — the player IS the human.  This struct tracks only
/// what the player has done and where they are.  All data is serialisable
/// so the full player state round-trips through save/load without loss.
struct Player {
    /// ID of the @ref Location where the player is currently standing.
    std::string current_location;

    /// @brief IDs of @ref Item objects in the player's inventory.
    ///
    /// Ordering is preserved (items appear in the order they were acquired).
    std::vector<std::string> inventory;

    /// @brief IDs of facts the player has learned through NPC interactions.
    ///
    /// Populated by the @c reveal_knowledge mutation when an NPC discloses a
    /// fact.  Used by @ref GameEngine to unlock certain player options or
    /// trigger resolution flags.
    std::vector<std::string> known_facts;

    /// @brief Number of times the player has spoken to each NPC.
    ///
    /// Keys are NPC IDs; values are encounter counts.  Used by
    /// @ref PromptBuilder and event conditions to reflect familiarity.
    std::unordered_map<std::string, int> npc_encounter_count;

    /// @brief Turns elapsed within the current active conversation.
    ///
    /// Reset to zero when a new conversation begins.  May be used by
    /// event conditions or prompt building to limit conversation length.
    int turns_in_conversation = 0;

    /// @brief Test whether the player currently holds a specific item.
    /// @param item_id The item ID to search for.
    /// @return @c true if @p item_id appears in @c inventory.
    bool has_item(const std::string &item_id) const;

    /// @brief Test whether the player has already learned a specific fact.
    /// @param fact_id The fact ID to search for.
    /// @return @c true if @p fact_id appears in @c known_facts.
    bool knows_fact(const std::string &fact_id) const;
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Player, current_location, inventory, known_facts,
                                                npc_encounter_count, turns_in_conversation)
/// @endcond

} // namespace chronicle
