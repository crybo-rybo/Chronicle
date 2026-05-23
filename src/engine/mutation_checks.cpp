/**
 * @file mutation_checks.cpp
 * @brief Non-mutating mutation precondition checks.
 */

#include "engine/mutation_checks.hpp"
#include "common/parse_utils.hpp"
#include "entities/npc.hpp"
#include <algorithm>

namespace chronicle {

namespace {

bool item_is_owned(const World &world, const std::string &item_id) {
    if (std::ranges::contains(world.player.inventory, item_id)) {
        return true;
    }
    for (const auto &[_, loc] : world.locations) {
        if (std::ranges::contains(loc.items, item_id)) {
            return true;
        }
    }
    for (const auto &[_, npc] : world.npcs) {
        if (std::ranges::contains(npc.state.inventory, item_id)) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> check_give_item_to_player(const World &world,
                                                     const MutationRequest &mutation) {
    auto item_id = param_value(mutation.params, "item_id");
    if (!item_id) {
        return "Error: missing item_id.";
    }
    auto npc_it = world.npcs.find(mutation.actor_id);
    if (npc_it == world.npcs.end() || !world.items.contains(*item_id)) {
        return "Error: invalid NPC or item.";
    }
    if (!std::ranges::contains(npc_it->second.state.inventory, *item_id)) {
        return "Error: NPC does not have that item.";
    }
    return std::nullopt;
}

std::optional<std::string> check_take_item_from_player(const World &world,
                                                       const MutationRequest &mutation) {
    auto item_id = param_value(mutation.params, "item_id");
    if (!item_id) {
        return "Error: missing item_id.";
    }
    auto npc_it = world.npcs.find(mutation.actor_id);
    if (npc_it == world.npcs.end() || !world.items.contains(*item_id)) {
        return "Error: invalid NPC or item.";
    }
    if (!std::ranges::contains(world.player.inventory, *item_id)) {
        return "Error: player does not have that item.";
    }
    return std::nullopt;
}

std::optional<std::string> check_update_npc_mood(const World &world,
                                                 const MutationRequest &mutation) {
    auto mood = param_value(mutation.params, "mood");
    if (!mood || !is_valid_mood(*mood)) {
        return "Error: invalid mood.";
    }
    auto npc_it = world.npcs.find(mutation.actor_id);
    if (npc_it == world.npcs.end()) {
        return "Error: NPC not found.";
    }
    if (npc_it->second.state.mood == *mood) {
        return "Error: mood is already set.";
    }
    return std::nullopt;
}

std::optional<std::string> check_update_npc_trust(const World &world,
                                                  const MutationRequest &mutation) {
    auto delta_param = param_value(mutation.params, "delta");
    if (!delta_param) {
        return "Error: missing trust delta.";
    }
    auto npc_it = world.npcs.find(mutation.actor_id);
    if (npc_it == world.npcs.end()) {
        return "Error: NPC not found.";
    }
    auto delta = parse_int(*delta_param);
    if (!delta) {
        return "Error: trust delta must be a valid integer.";
    }
    const auto &trust = npc_it->second.state.trust_toward_player;
    if (std::clamp(trust + *delta, -100, 100) == trust) {
        return "Error: trust would not change.";
    }
    return std::nullopt;
}

std::optional<std::string> check_move_npc(const World &world, const MutationRequest &mutation) {
    auto location_id = param_value(mutation.params, "location_id");
    if (!location_id) {
        return "Error: missing location_id.";
    }
    auto npc_it = world.npcs.find(mutation.actor_id);
    auto new_loc_it = world.locations.find(*location_id);
    if (npc_it == world.npcs.end() || new_loc_it == world.locations.end()) {
        return "Error: invalid NPC or location.";
    }
    const auto &npc_id = mutation.actor_id;
    const auto &npc_state = npc_it->second.state;
    if (npc_state.current_location == *location_id &&
        std::ranges::contains(new_loc_it->second.npcs, npc_id)) {
        return "Error: NPC is already at that location.";
    }
    return std::nullopt;
}

std::optional<std::string> check_reveal_knowledge(const World &world,
                                                  const MutationRequest &mutation) {
    auto fact_id = param_value(mutation.params, "fact_id");
    if (!fact_id || !world.npcs.contains(mutation.actor_id)) {
        return "Error: invalid fact or NPC.";
    }
    if (std::ranges::contains(world.player.known_facts, *fact_id)) {
        return "Error: fact is already known.";
    }
    return std::nullopt;
}

std::optional<std::string> check_add_memory(const World &world, const MutationRequest &mutation) {
    auto summary = param_value(mutation.params, "summary");
    auto importance_param = param_value(mutation.params, "importance");
    if (!summary || !importance_param) {
        return "Error: missing memory summary or importance.";
    }
    if (!world.npcs.contains(mutation.actor_id)) {
        return "Error: NPC not found.";
    }
    if (!parse_int(*importance_param)) {
        return "Error: importance must be a valid integer.";
    }
    return std::nullopt;
}

std::optional<std::string> check_set_flag(const World &world, const MutationRequest &mutation) {
    auto flag_id = param_value(mutation.params, "flag_id");
    auto value_param = param_value(mutation.params, "value");
    if (!flag_id || !value_param) {
        return "Error: missing flag_id or value.";
    }
    auto flag_it = world.flags.find(*flag_id);
    if (flag_it == world.flags.end()) {
        return "Error: flag not found.";
    }
    auto value = parse_bool(*value_param);
    if (!value) {
        return "Error: flag value must be true or false.";
    }
    if (flag_it->second == *value) {
        return "Error: flag is already set.";
    }
    return std::nullopt;
}

std::optional<std::string> check_player_move(const World &world, const MutationRequest &mutation) {
    auto location_id = param_value(mutation.params, "location_id");
    if (!location_id || !world.locations.contains(*location_id)) {
        return "Error: invalid destination.";
    }
    return std::nullopt;
}

std::optional<std::string> check_player_take(const World &world, const MutationRequest &mutation) {
    auto item_id = param_value(mutation.params, "item_id");
    if (!item_id || !world.items.contains(*item_id)) {
        return "Error: invalid item.";
    }
    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return "Error: player location invalid.";
    }
    if (!std::ranges::contains(loc_it->second.items, *item_id)) {
        return "Error: item is not here.";
    }
    return std::nullopt;
}

std::optional<std::string> check_player_drop(const World &world, const MutationRequest &mutation) {
    auto item_id = param_value(mutation.params, "item_id");
    if (!item_id || !world.items.contains(*item_id)) {
        return "Error: invalid item.";
    }
    if (!std::ranges::contains(world.player.inventory, *item_id)) {
        return "Error: player is not carrying that item.";
    }
    return std::nullopt;
}

std::optional<std::string> check_unlock_exit(const World &world, const MutationRequest &mutation) {
    auto location_id = param_value(mutation.params, "location_id");
    auto direction = param_value(mutation.params, "direction");
    if (!location_id || !direction) {
        return "Error: missing location or direction.";
    }
    auto loc_it = world.locations.find(*location_id);
    if (loc_it == world.locations.end()) {
        return "Error: location not found.";
    }
    if (!std::ranges::contains(loc_it->second.locked_exits, *direction)) {
        return "Error: exit is not locked.";
    }
    return std::nullopt;
}

std::optional<std::string> check_spawn_item(const World &world, const MutationRequest &mutation) {
    auto item_id = param_value(mutation.params, "item_id");
    auto location_id = param_value(mutation.params, "location_id");
    if (!item_id || !location_id) {
        return "Error: missing item_id or location_id.";
    }
    if (!world.locations.contains(*location_id) || !world.items.contains(*item_id)) {
        return "Error: invalid item or location.";
    }
    if (item_is_owned(world, *item_id)) {
        return "Error: item is already owned.";
    }
    return std::nullopt;
}

} // namespace

std::optional<std::string> check_mutation(const World &world, const MutationRequest &mutation) {
    switch (mutation.type) {
    case MutationRequest::Type::GiveItemToPlayer:
        return check_give_item_to_player(world, mutation);
    case MutationRequest::Type::TakeItemFromPlayer:
        return check_take_item_from_player(world, mutation);
    case MutationRequest::Type::UpdateNpcMood:
        return check_update_npc_mood(world, mutation);
    case MutationRequest::Type::UpdateNpcTrust:
        return check_update_npc_trust(world, mutation);
    case MutationRequest::Type::MoveNpc:
        return check_move_npc(world, mutation);
    case MutationRequest::Type::RevealKnowledge:
        return check_reveal_knowledge(world, mutation);
    case MutationRequest::Type::AddMemory:
        return check_add_memory(world, mutation);
    case MutationRequest::Type::SetFlag:
        return check_set_flag(world, mutation);
    case MutationRequest::Type::PlayerMove:
        return check_player_move(world, mutation);
    case MutationRequest::Type::PlayerTake:
        return check_player_take(world, mutation);
    case MutationRequest::Type::PlayerDrop:
        return check_player_drop(world, mutation);
    case MutationRequest::Type::UnlockExit:
        return check_unlock_exit(world, mutation);
    case MutationRequest::Type::SpawnItem:
        return check_spawn_item(world, mutation);
    }
    return "Error: unknown mutation type.";
}

} // namespace chronicle
