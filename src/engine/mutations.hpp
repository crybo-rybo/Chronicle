#pragma once
#include "ai/tool_registry.hpp"
#include "entities/world.hpp"

namespace chronicle {

// Applies a mutation deterministically to the world state.
// These functions assume the mutation request has already been validated
// by ToolRegistry.

bool apply_give_item_to_player(World &world, const MutationRequest &mutation);
bool apply_take_item_from_player(World &world, const MutationRequest &mutation);
bool apply_update_npc_mood(World &world, const MutationRequest &mutation);
bool apply_update_npc_trust(World &world, const MutationRequest &mutation);
bool apply_move_npc(World &world, const MutationRequest &mutation);
bool apply_reveal_knowledge(World &world, const MutationRequest &mutation);
bool apply_add_memory(World &world, const MutationRequest &mutation);
bool apply_set_flag(World &world, const MutationRequest &mutation);

// Dispatcher
bool apply_mutation(World &world, const MutationRequest &mutation);

} // namespace chronicle
