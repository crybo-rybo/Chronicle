/**
 * @file world.hpp
 * @brief Top-level world state container for Chronicle.
 *
 * @details @ref World is the single source of truth for all game state.
 * It is a plain data aggregate — it has no behaviour of its own.  Every other
 * subsystem that needs to read world state accepts a @c const @c World&; every
 * subsystem that needs to write world state does so through the
 * @ref GameEngine mutation pipeline, which is the only permitted write path.
 *
 * All fields are serialisable to JSON so the entire world can be saved and
 * restored without loss.  Entities reference each other exclusively by string
 * ID, keeping the struct free of raw pointers or runtime-only references.
 */

#pragma once
#include "entities/clock.hpp"
#include "entities/event.hpp"
#include "entities/fact.hpp"
#include "entities/item.hpp"
#include "entities/location.hpp"
#include "entities/npc.hpp"
#include "entities/player.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace chronicle {

/// @brief The complete, authoritative state of the game world.
///
/// @details This struct owns every piece of persistent game data.  It is
/// populated at startup by the world loader from JSON data files and can be
/// serialised in its entirety to a save file.
///
/// @note No subsystem other than @ref GameEngine may write to a @ref World
/// instance.  All state changes — including those triggered by LLM tool calls
/// — are submitted as @ref MutationRequest objects and applied by the engine
/// in a deterministic, validated order.
struct World {
    /// The in-game clock tracking current day and time period.
    Clock clock;

    /// @brief All navigable locations, keyed by location ID.
    ///
    /// The location graph is static (connections do not change at runtime),
    /// but each location's NPC and item lists are mutated as characters and
    /// objects move through the world.
    std::unordered_map<std::string, Location> locations;

    /// All NPCs in the game world, keyed by NPC ID.
    std::unordered_map<std::string, Npc> npcs;

    /// @brief Canonical item registry, keyed by item ID.
    ///
    /// Every @ref Item has exactly one entry here.  Inventory membership is
    /// tracked separately in @ref Player::inventory, @ref NpcState::inventory,
    /// and @ref Location::items.
    std::unordered_map<std::string, Item> items;

    /// The player character's current state.
    Player player;

    /// @brief Authored fact registry, keyed by fact ID.
    ///
    /// Facts are loaded from @c facts.json and referenced by NPC knowledge
    /// lists.  The prompt builder resolves fact IDs to authored text here.
    std::unordered_map<std::string, Fact> facts;

    /// @brief Global narrative flags, keyed by flag ID.
    ///
    /// Boolean flags used to track resolution-relevant milestones (e.g.
    /// @c "discovered_secret", @c "villain_exposed").  NPCs can set flags via
    /// the @c set_flag tool call; event triggers can test them via
    /// @ref ConditionType::FlagSet.
    std::unordered_map<std::string, bool> flags;

    /// @brief Scripted event triggers.
    ///
    /// Stored as a vector rather than a map because the ID is embedded in
    /// each @ref EventTrigger struct and triggers are always iterated in full.
    std::vector<EventTrigger> events;

    /// @brief Total number of significant turns elapsed across the whole game.
    ///
    /// Incremented by @ref Clock::advance_turn and tested by
    /// @ref ConditionType::TurnGe.
    int total_turns_elapsed = 0;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const World &w);
void from_json(const nlohmann::json &j, World &w);
/// @endcond

} // namespace chronicle
