/**
 * @file player.cpp
 * @brief Implementation of @ref Player helper methods.
 */

#include "entities/player.hpp"

#include <algorithm>

namespace chronicle {

bool Player::has_item(const std::string& item_id) const {
    return std::ranges::find(inventory, item_id) != inventory.end();
}

bool Player::knows_fact(const std::string& fact_id) const {
    return std::ranges::find(known_facts, fact_id) != known_facts.end();
}

} // namespace chronicle
