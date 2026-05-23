/**
 * @file world_query.hpp
 * @brief Read-only world lookup helpers for player commands and validators.
 */

#pragma once
#include "entities/world.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace chronicle {

struct LockedExitMatch {
    std::string direction;
    std::string destination_id;
    std::string destination_name;
};

bool player_can_see_item(const World &world, const std::string &item_id);
bool player_can_see_npc(const World &world, const std::string &npc_id);

std::optional<std::string> find_visible_npc_id(const World &world, std::string_view query);
std::optional<std::string> find_inventory_item_id(const World &world, std::string_view query);
std::optional<std::string> find_takeable_item_in_location(const World &world, std::string_view query);
std::optional<std::string> find_accessible_item_id(const World &world, std::string_view query);
std::optional<LockedExitMatch> find_locked_exit_match(const World &world, std::string_view query);

} // namespace chronicle
