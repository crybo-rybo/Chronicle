/**
 * @file world_query.cpp
 * @brief Implementation of read-only @ref World lookup helpers.
 */

#include "engine/world_query.hpp"
#include "engine/text_utils.hpp"
#include <algorithm>

namespace chronicle {

bool player_can_see_item(const World &world, const std::string &item_id) {
    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return false;
    }
    return std::ranges::contains(loc_it->second.items, item_id);
}

bool player_can_see_npc(const World &world, const std::string &npc_id) {
    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return false;
    }
    return std::ranges::contains(loc_it->second.npcs, npc_id);
}

std::optional<std::string> find_visible_npc_id(const World &world, std::string_view query) {
    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return std::nullopt;
    }

    for (const auto &npc_id : loc_it->second.npcs) {
        auto npc_it = world.npcs.find(npc_id);
        if (npc_it != world.npcs.end() &&
            (text::contains_normalized(npc_it->second.identity.name, query) ||
             text::contains_normalized(npc_id, query))) {
            return npc_id;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_inventory_item_id(const World &world, std::string_view query) {
    for (const auto &item_id : world.player.inventory) {
        auto item_it = world.items.find(item_id);
        if (item_it != world.items.end() &&
            (text::contains_normalized(item_it->second.name, query) ||
             text::contains_normalized(item_id, query))) {
            return item_id;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_takeable_item_in_location(const World &world,
                                                          std::string_view query) {
    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return std::nullopt;
    }

    for (const auto &item_id : loc_it->second.items) {
        auto item_it = world.items.find(item_id);
        if (item_it != world.items.end() && !item_it->second.hidden && item_it->second.takeable &&
            (text::contains_normalized(item_it->second.name, query) ||
             text::contains_normalized(item_id, query))) {
            return item_id;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_accessible_item_id(const World &world, std::string_view query) {
    if (auto inv_match = find_inventory_item_id(world, query)) {
        return inv_match;
    }

    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return std::nullopt;
    }

    for (const auto &item_id : loc_it->second.items) {
        auto item_it = world.items.find(item_id);
        if (item_it != world.items.end() && !item_it->second.hidden &&
            (text::contains_normalized(item_it->second.name, query) ||
             text::contains_normalized(item_id, query))) {
            return item_id;
        }
    }
    return std::nullopt;
}

std::optional<LockedExitMatch> find_locked_exit_match(const World &world, std::string_view query) {
    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return std::nullopt;
    }

    const auto &loc = loc_it->second;
    for (const auto &direction : loc.locked_exits) {
        auto exit_it = loc.exits.find(direction);
        if (exit_it == loc.exits.end()) {
            continue;
        }
        const auto &destination_id = exit_it->second;
        std::string destination_name = destination_id;
        if (auto dest_it = world.locations.find(destination_id); dest_it != world.locations.end()) {
            destination_name = dest_it->second.name;
        }

        if (text::contains_normalized(direction, query) ||
            text::contains_normalized(destination_id, query) ||
            text::contains_normalized(destination_name, query)) {
            return LockedExitMatch{direction, destination_id, destination_name};
        }
    }
    return std::nullopt;
}

} // namespace chronicle
