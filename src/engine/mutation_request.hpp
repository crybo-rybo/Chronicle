/**
 * @file mutation_request.hpp
 * @brief Domain-owned mutation contract for all state changes to the World.
 *
 * @details @ref MutationRequest is the single type through which player commands,
 * NPC tool calls, and system/event actions express proposed world mutations.
 * All producers (AI, player, events) create these; only @ref GameEngine applies
 * them.
 */

#pragma once
#include <functional>
#include <map>
#include <string>

namespace chronicle {

/// @brief Describes a single validated, pending state change to the world.
///
/// @details Produced by @ref ToolRegistry (NPC), mutation gate validators
/// (player), and event adapters (system).  Consumed by the @ref apply_mutation
/// dispatcher in @c mutations.hpp.  All parameters are stored as strings to
/// keep the type serialisation-friendly.
struct MutationRequest {
    /// @brief Who initiated the mutation.
    enum class Source {
        Player, ///< Player command (go, take, drop, use).
        Npc,    ///< NPC tool call during inference.
        System  ///< Engine event or scripted action.
    };

    /// @brief Enumeration of all possible state change types.
    ///
    /// @details Each type has a small string-keyed parameter contract:
    /// - @c GiveItemToPlayer: @c item_id.
    /// - @c TakeItemFromPlayer: @c item_id.
    /// - @c UpdateNpcMood: @c mood.
    /// - @c UpdateNpcTrust: @c delta as a signed integer string.
    /// - @c MoveNpc: @c location_id.
    /// - @c RevealKnowledge: @c fact_id.
    /// - @c AddMemory: @c summary and @c importance as an integer string.
    /// - @c SetFlag: @c flag_id and @c value as @c "true" or @c "false".
    /// - @c PlayerMove: @c location_id and @c direction.
    /// - @c PlayerTake: @c item_id.
    /// - @c PlayerDrop: @c item_id.
    /// - @c UnlockExit: @c location_id and @c direction.
    /// - @c SpawnItem: @c item_id and @c location_id.
    enum class Type {
        // NPC-originated
        GiveItemToPlayer,   ///< Transfer an item from an NPC to the player.
        TakeItemFromPlayer, ///< Transfer an item from the player to an NPC.
        UpdateNpcMood,      ///< Change an NPC's emotional state.
        UpdateNpcTrust,     ///< Adjust an NPC's trust toward the player.
        MoveNpc,            ///< Move an NPC to a different location.
        RevealKnowledge,    ///< Add a fact to the player's known-facts list.
        AddMemory,          ///< Append a @ref MemoryEntry to an NPC's memory list.
        SetFlag,            ///< Set a global narrative flag to a boolean value.

        // Player-originated
        PlayerMove, ///< Move the player to a connected location.
        PlayerTake, ///< Take an item from the current location.
        PlayerDrop, ///< Drop an item from the player's inventory.
        UnlockExit, ///< Unlock a locked exit in the current location.

        // System/event-originated
        SpawnItem, ///< Create or place an item in a location.
    };

    Type type;                   ///< The kind of state change to perform.
    Source source = Source::Npc; ///< Who initiated the change.
    std::string actor_id;        ///< NPC id, "player", or system source id.

    /// @brief Type-specific named parameters.
    ///
    /// @details Values are stored as strings so every producer can share the
    /// same mutation type without templated payload variants.  Validators are
    /// responsible for producing the keys documented in @ref Type, and
    /// application functions still fail gracefully if stale or malformed
    /// parameters reach the dispatcher.
    std::map<std::string, std::string> params;
};

/// @brief Callback type for sinking validated mutations into a queue.
///
/// @details Used by validation layers such as @ref ToolRegistry to submit
/// accepted requests to @ref GameEngine without gaining direct write access to
/// @ref World.
using MutationSink = std::function<void(MutationRequest)>;

} // namespace chronicle
