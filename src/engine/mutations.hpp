/**
 * @file mutations.hpp
 * @brief World-state mutation functions for Chronicle.
 *
 * @details This file declares the functions that physically apply validated
 * @ref MutationRequest objects to a @ref World instance.  All functions in
 * this file assume that the mutation has already been validated by
 * @ref ToolRegistry; they perform lightweight runtime checks but do not
 * produce user-facing error messages.
 *
 * The @ref apply_mutation dispatcher is the single call site used by
 * @ref GameEngine::process_pending_mutations to drain the pending queue after
 * each turn.
 *
 * @note These functions are the only code outside of @ref GameEngine that
 * writes to @ref World.  They must not be called from any other context.
 */

#pragma once
#include "engine/mutation_request.hpp"
#include "entities/world.hpp"

namespace chronicle {

/// @brief Transfer an item from an NPC's inventory to the player's inventory.
///
/// @details Removes the item from the NPC's inventory list and appends it to
/// the player's inventory if not already present (duplicate guard via
/// @c add_unique).
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c GiveItemToPlayer and
///                 @c params["item_id"] set to a valid item ID.
/// @return @c true if the mutation was applied; @c false if a precondition
///         failed (NPC not found, item not in NPC inventory, etc.).
bool apply_give_item_to_player(World &world, const MutationRequest &mutation);

/// @brief Transfer an item from the player's inventory to an NPC's inventory.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c TakeItemFromPlayer and
///                 @c params["item_id"] set to a valid, non-key item ID.
/// @return @c true if the mutation was applied.
bool apply_take_item_from_player(World &world, const MutationRequest &mutation);

/// @brief Update the mood field of an NPC.
///
/// @details Returns @c false (no-op) if the mood is already the requested
/// value, avoiding spurious narration of a non-change.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c UpdateNpcMood and
///                 @c params["mood"] set to a valid mood string.
/// @return @c true if the mood changed.
bool apply_update_npc_mood(World &world, const MutationRequest &mutation);

/// @brief Adjust the NPC's trust toward the player by a signed delta.
///
/// @details The new trust value is clamped to [-100, 100].  Returns @c false
/// if the resulting value would equal the current value (already at boundary).
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c UpdateNpcTrust and
///                 @c params["delta"] set to a valid integer string.
/// @return @c true if the trust value changed.
bool apply_update_npc_trust(World &world, const MutationRequest &mutation);

/// @brief Move an NPC from their current location to a new location.
///
/// @details Removes the NPC ID from the old location's NPC list and appends
/// it to the new location's NPC list.  Also updates
/// @c NpcState::current_location.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c MoveNpc and
///                 @c params["location_id"] set to an existing location ID.
/// @return @c true if the NPC was moved; @c false if they are already at the
///         destination.
bool apply_move_npc(World &world, const MutationRequest &mutation);

/// @brief Add a fact ID to the player's known-facts list.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c RevealKnowledge and
///                 @c params["fact_id"] set.
/// @return @c true if the fact was added (i.e., it was not already known).
bool apply_reveal_knowledge(World &world, const MutationRequest &mutation);

/// @brief Append a new @ref MemoryEntry to an NPC's memory list.
///
/// @details Constructs the memory entry from the mutation parameters, using
/// the current clock time as the timestamp and clamping importance to [1, 10].
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c AddMemory,
///                 @c params["summary"] non-empty, and
///                 @c params["importance"] a valid integer string.
/// @return @c true if the memory was added.
bool apply_add_memory(World &world, const MutationRequest &mutation);

/// @brief Set a global narrative flag to a boolean value.
///
/// @details Returns @c false (no-op) if the flag is already at the requested
/// value.  Only flags that exist in @ref World::flags can be modified.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c SetFlag,
///                 @c params["flag_id"] matching a key in @c world.flags, and
///                 @c params["value"] equal to @c "true" or @c "false".
/// @return @c true if the flag value changed.
bool apply_set_flag(World &world, const MutationRequest &mutation);

/// @brief Move the player to a connected location.
///
/// @details Updates @ref Player::current_location.  The validator supplies
/// both @c location_id and @c direction; only @c location_id is required for
/// application.  Returns @c false if the destination no longer exists.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c PlayerMove and
///                 @c params["location_id"] set to an existing location ID.
/// @return @c true if the player's location was updated.
bool apply_player_move(World &world, const MutationRequest &mutation);

/// @brief Take an item from the current location into the player's inventory.
///
/// @details Removes @c item_id from the current location's item list and adds
/// it to @ref Player::inventory.  Returns @c false if the player is no longer
/// in a valid location, the item no longer exists, or another mutation already
/// removed the item from the room.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c PlayerTake and
///                 @c params["item_id"] set to an existing item ID.
/// @return @c true if ownership changed.
bool apply_player_take(World &world, const MutationRequest &mutation);

/// @brief Drop an item from the player's inventory into the current location.
///
/// @details Removes @c item_id from @ref Player::inventory and places it in
/// the current location if the location still exists.  Key-item restrictions
/// are enforced by the validator; this function still returns @c false if the
/// player no longer owns the item.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c PlayerDrop and
///                 @c params["item_id"] set to an existing item ID.
/// @return @c true if the item left the player's inventory.
bool apply_player_drop(World &world, const MutationRequest &mutation);

/// @brief Place an item into a location (system/event use).
///
/// @details Places an existing registry item into a location only when that
/// item is currently unowned by the player, all NPCs, and all locations.  This
/// keeps @ref World::items as the canonical registry while preserving the
/// single-owner invariant.
///
/// @param world    The world to mutate.
/// @param mutation Must have @c type == @c SpawnItem,
///                 @c params["item_id"] set to an existing item ID, and
///                 @c params["location_id"] set to an existing location ID.
/// @return @c true if the item was placed in the location.
bool apply_spawn_item(World &world, const MutationRequest &mutation);

/// @brief Dispatch a @ref MutationRequest to the appropriate apply function.
///
/// @details Selects the correct apply function based on @c mutation.type and
/// forwards the call.  This is the single entry point used by
/// @ref GameEngine::process_pending_mutations.
///
/// @param world    The world to mutate.
/// @param mutation The validated mutation to apply.
/// @return @c true if the mutation produced a state change.
bool apply_mutation(World &world, const MutationRequest &mutation);

} // namespace chronicle
