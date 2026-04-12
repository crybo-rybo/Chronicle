#include "engine/mutations.hpp"
#include <algorithm>

namespace chronicle {

void apply_give_item_to_player(World &world, const MutationRequest &mutation) {
    auto item_id = mutation.params.at("item_id");
    auto &npc_inv = world.npcs.at(mutation.npc_id).state.inventory;
    auto &player_inv = world.player.inventory;

    auto it = std::ranges::find(npc_inv, item_id);
    if (it != npc_inv.end()) {
        npc_inv.erase(it);
        player_inv.push_back(item_id);
    }
}

void apply_take_item_from_player(World &world, const MutationRequest &mutation) {
    auto item_id = mutation.params.at("item_id");
    auto &npc_inv = world.npcs.at(mutation.npc_id).state.inventory;
    auto &player_inv = world.player.inventory;

    auto it = std::ranges::find(player_inv, item_id);
    if (it != player_inv.end()) {
        player_inv.erase(it);
        npc_inv.push_back(item_id);
    }
}

void apply_update_npc_mood(World &world, const MutationRequest &mutation) {
    auto mood = mutation.params.at("mood");
    world.npcs.at(mutation.npc_id).state.mood = mood;
}

void apply_update_npc_trust(World &world, const MutationRequest &mutation) {
    int delta = std::stoi(mutation.params.at("delta"));
    int current = world.npcs.at(mutation.npc_id).state.trust_toward_player;
    world.npcs.at(mutation.npc_id).state.trust_toward_player = std::clamp(current + delta, -100, 100);
}

void apply_move_npc(World &world, const MutationRequest &mutation) {
    auto location_id = mutation.params.at("location_id");
    auto npc_id = mutation.npc_id;

    // Remove from old location
    auto old_location_id = world.npcs.at(npc_id).state.current_location;
    if (world.locations.contains(old_location_id)) {
        auto &old_npcs = world.locations.at(old_location_id).npcs;
        auto it = std::ranges::find(old_npcs, npc_id);
        if (it != old_npcs.end()) {
            old_npcs.erase(it);
        }
    }

    // Add to new location
    world.npcs.at(npc_id).state.current_location = location_id;
    if (world.locations.contains(location_id)) {
        auto &new_npcs = world.locations.at(location_id).npcs;
        if (std::ranges::find(new_npcs, npc_id) == new_npcs.end()) {
            new_npcs.push_back(npc_id);
        }
    }
}

void apply_reveal_knowledge(World &world, const MutationRequest &mutation) {
    auto fact_id = mutation.params.at("fact_id");
    if (std::ranges::find(world.player.known_facts, fact_id) == world.player.known_facts.end()) {
        world.player.known_facts.push_back(fact_id);
    }
}

void apply_add_memory(World &world, const MutationRequest &mutation) {
    auto summary = mutation.params.at("summary");
    int importance = std::stoi(mutation.params.at("importance"));
    
    MemoryEntry mem;
    mem.timestamp = world.clock.to_prompt_string();
    mem.type = "observation";
    mem.summary = summary;
    mem.importance = importance;

    world.npcs.at(mutation.npc_id).state.memories.push_back(mem);
}

void apply_set_flag(World &world, const MutationRequest &mutation) {
    auto flag_id = mutation.params.at("flag_id");
    bool value = (mutation.params.at("value") == "true");
    world.flags[flag_id] = value;
}

void apply_mutation(World &world, const MutationRequest &mutation) {
    switch (mutation.type) {
    case MutationRequest::Type::GiveItemToPlayer:
        apply_give_item_to_player(world, mutation);
        break;
    case MutationRequest::Type::TakeItemFromPlayer:
        apply_take_item_from_player(world, mutation);
        break;
    case MutationRequest::Type::UpdateNpcMood:
        apply_update_npc_mood(world, mutation);
        break;
    case MutationRequest::Type::UpdateNpcTrust:
        apply_update_npc_trust(world, mutation);
        break;
    case MutationRequest::Type::MoveNpc:
        apply_move_npc(world, mutation);
        break;
    case MutationRequest::Type::RevealKnowledge:
        apply_reveal_knowledge(world, mutation);
        break;
    case MutationRequest::Type::AddMemory:
        apply_add_memory(world, mutation);
        break;
    case MutationRequest::Type::SetFlag:
        apply_set_flag(world, mutation);
        break;
    }
}

} // namespace chronicle
