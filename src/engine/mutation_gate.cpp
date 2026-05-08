/**
 * @file mutation_gate.cpp
 * @brief Implementation of player-command mutation validators.
 */

#include "engine/mutation_gate.hpp"
#include "engine/text_utils.hpp"
#include <algorithm>

namespace chronicle {

std::variant<MutationRequest, std::string> validate_player_move(const World &world,
                                                                std::string_view direction) {
    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return std::string("You aren't anywhere.");
    }
    auto exit_it = loc_it->second.exits.find(std::string(direction));
    if (exit_it == loc_it->second.exits.end()) {
        return std::string("You can't go that way.");
    }
    if (std::ranges::contains(loc_it->second.locked_exits, std::string(direction))) {
        return "The way " + std::string(direction) + " is locked.";
    }
    return MutationRequest{
        MutationRequest::Type::PlayerMove,
        MutationRequest::Source::Player,
        "player",
        {{"location_id", exit_it->second}, {"direction", std::string(direction)}}};
}

std::variant<MutationRequest, std::string> validate_player_take(const World &world,
                                                                std::string_view item_query) {
    auto loc_it = world.locations.find(world.player.current_location);
    if (loc_it == world.locations.end()) {
        return std::string("You don't see that here.");
    }
    const auto &items = loc_it->second.items;
    auto item_it = std::ranges::find_if(items, [&](const std::string &id) {
        auto w_it = world.items.find(id);
        if (w_it != world.items.end()) {
            return !w_it->second.hidden && w_it->second.takeable &&
                   (text::contains_normalized(w_it->second.name, item_query) ||
                    text::contains_normalized(id, item_query));
        }
        return false;
    });
    if (item_it == items.end()) {
        return std::string("You don't see that here.");
    }
    return MutationRequest{MutationRequest::Type::PlayerTake,
                           MutationRequest::Source::Player,
                           "player",
                           {{"item_id", *item_it}}};
}

std::variant<MutationRequest, std::string> validate_player_drop(const World &world,
                                                                std::string_view item_query) {
    const auto &inv = world.player.inventory;
    auto item_it = std::ranges::find_if(inv, [&](const std::string &id) {
        auto w_it = world.items.find(id);
        if (w_it != world.items.end()) {
            return text::contains_normalized(w_it->second.name, item_query) ||
                   text::contains_normalized(id, item_query);
        }
        return false;
    });
    if (item_it == inv.end()) {
        return std::string("You aren't carrying that.");
    }
    auto w_it = world.items.find(*item_it);
    if (w_it != world.items.end() && w_it->second.key_item) {
        return std::string("You can't drop that \u2014 it's too important to leave behind.");
    }
    return MutationRequest{MutationRequest::Type::PlayerDrop,
                           MutationRequest::Source::Player,
                           "player",
                           {{"item_id", *item_it}}};
}

} // namespace chronicle
