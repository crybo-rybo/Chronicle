/**
 * @file mutations.cpp
 * @brief Implementation of world-state mutation functions.
 *
 * @details Each @c apply_* function applies a single validated
 * @ref MutationRequest to the given @ref World.  Internal helpers
 * (@c get_param, @c parse_int, @c parse_bool, @c add_unique) centralise
 * parameter extraction and deduplication logic shared across multiple
 * mutation types.
 */

#include "engine/mutations.hpp"
#include <algorithm>
#include <optional>

namespace chronicle {

namespace {

std::optional<std::string> get_param(const MutationRequest &mutation, const std::string &key) {
    auto it = mutation.params.find(key);
    if (it == mutation.params.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<int> parse_int(const std::string &value) {
    try {
        std::size_t parsed = 0;
        int result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

std::optional<bool> parse_bool(const std::string &value) {
    if (value == "true" || value == "1") {
        return true;
    }
    if (value == "false" || value == "0") {
        return false;
    }
    return std::nullopt;
}

bool add_unique(std::vector<std::string> &values, const std::string &value) {
    if (std::ranges::contains(values, value)) {
        return false;
    }
    values.push_back(value);
    return true;
}

} // namespace

bool apply_give_item_to_player(World &world, const MutationRequest &mutation) {
    auto item_id = get_param(mutation, "item_id");
    if (!item_id) {
        return false;
    }
    auto npc_it = world.npcs.find(mutation.npc_id);
    if (npc_it == world.npcs.end() || !world.items.contains(*item_id)) {
        return false;
    }

    auto &npc_inv = npc_it->second.state.inventory;
    auto &player_inv = world.player.inventory;

    auto it = std::ranges::find(npc_inv, *item_id);
    if (it == npc_inv.end()) {
        return false;
    }
    npc_inv.erase(it);
    add_unique(player_inv, *item_id);
    return true;
}

bool apply_take_item_from_player(World &world, const MutationRequest &mutation) {
    auto item_id = get_param(mutation, "item_id");
    if (!item_id) {
        return false;
    }
    auto npc_it = world.npcs.find(mutation.npc_id);
    if (npc_it == world.npcs.end() || !world.items.contains(*item_id)) {
        return false;
    }

    auto &npc_inv = npc_it->second.state.inventory;
    auto &player_inv = world.player.inventory;

    auto it = std::ranges::find(player_inv, *item_id);
    if (it == player_inv.end()) {
        return false;
    }
    player_inv.erase(it);
    add_unique(npc_inv, *item_id);
    return true;
}

bool apply_update_npc_mood(World &world, const MutationRequest &mutation) {
    auto mood = get_param(mutation, "mood");
    if (!mood) {
        return false;
    }
    auto npc_it = world.npcs.find(mutation.npc_id);
    if (npc_it == world.npcs.end()) {
        return false;
    }
    auto &current = npc_it->second.state.mood;
    if (current == *mood) {
        return false;
    }
    current = *mood;
    return true;
}

bool apply_update_npc_trust(World &world, const MutationRequest &mutation) {
    auto delta_param = get_param(mutation, "delta");
    if (!delta_param) {
        return false;
    }
    auto npc_it = world.npcs.find(mutation.npc_id);
    if (npc_it == world.npcs.end()) {
        return false;
    }
    auto delta = parse_int(*delta_param);
    if (!delta) {
        return false;
    }
    auto &trust = npc_it->second.state.trust_toward_player;
    int updated = std::clamp(trust + *delta, -100, 100);
    if (updated == trust) {
        return false;
    }
    trust = updated;
    return true;
}

bool apply_move_npc(World &world, const MutationRequest &mutation) {
    auto location_id = get_param(mutation, "location_id");
    if (!location_id) {
        return false;
    }
    auto npc_it = world.npcs.find(mutation.npc_id);
    auto new_loc_it = world.locations.find(*location_id);
    if (npc_it == world.npcs.end() || new_loc_it == world.locations.end()) {
        return false;
    }

    const auto &npc_id = mutation.npc_id;
    auto &npc_state = npc_it->second.state;
    if (npc_state.current_location == *location_id &&
        std::ranges::contains(new_loc_it->second.npcs, npc_id)) {
        return false;
    }

    // Remove from old location
    if (auto old_loc_it = world.locations.find(npc_state.current_location);
        old_loc_it != world.locations.end()) {
        auto &old_npcs = old_loc_it->second.npcs;
        if (auto it = std::ranges::find(old_npcs, npc_id); it != old_npcs.end()) {
            old_npcs.erase(it);
        }
    }

    // Add to new location
    npc_state.current_location = *location_id;
    add_unique(new_loc_it->second.npcs, npc_id);
    return true;
}

bool apply_reveal_knowledge(World &world, const MutationRequest &mutation) {
    auto fact_id = get_param(mutation, "fact_id");
    if (!fact_id || !world.npcs.contains(mutation.npc_id)) {
        return false;
    }
    return add_unique(world.player.known_facts, *fact_id);
}

bool apply_add_memory(World &world, const MutationRequest &mutation) {
    auto summary = get_param(mutation, "summary");
    auto importance_param = get_param(mutation, "importance");
    if (!summary || !importance_param) {
        return false;
    }
    auto npc_it = world.npcs.find(mutation.npc_id);
    if (npc_it == world.npcs.end()) {
        return false;
    }
    auto importance = parse_int(*importance_param);
    if (!importance) {
        return false;
    }

    MemoryEntry mem;
    mem.timestamp = world.clock.to_prompt_string();
    mem.type = "observation";
    mem.summary = std::move(*summary);
    mem.importance = std::clamp(*importance, 1, 10);

    npc_it->second.state.memories.push_back(std::move(mem));
    return true;
}

bool apply_set_flag(World &world, const MutationRequest &mutation) {
    auto flag_id = get_param(mutation, "flag_id");
    auto value_param = get_param(mutation, "value");
    if (!flag_id || !value_param) {
        return false;
    }
    auto flag_it = world.flags.find(*flag_id);
    if (flag_it == world.flags.end()) {
        return false;
    }
    auto value = parse_bool(*value_param);
    if (!value) {
        return false;
    }
    if (flag_it->second == *value) {
        return false;
    }
    flag_it->second = *value;
    return true;
}

bool apply_mutation(World &world, const MutationRequest &mutation) {
    switch (mutation.type) {
    case MutationRequest::Type::GiveItemToPlayer:
        return apply_give_item_to_player(world, mutation);
    case MutationRequest::Type::TakeItemFromPlayer:
        return apply_take_item_from_player(world, mutation);
    case MutationRequest::Type::UpdateNpcMood:
        return apply_update_npc_mood(world, mutation);
    case MutationRequest::Type::UpdateNpcTrust:
        return apply_update_npc_trust(world, mutation);
    case MutationRequest::Type::MoveNpc:
        return apply_move_npc(world, mutation);
    case MutationRequest::Type::RevealKnowledge:
        return apply_reveal_knowledge(world, mutation);
    case MutationRequest::Type::AddMemory:
        return apply_add_memory(world, mutation);
    case MutationRequest::Type::SetFlag:
        return apply_set_flag(world, mutation);
    }
    return false;
}

} // namespace chronicle
