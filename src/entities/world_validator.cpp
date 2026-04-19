/**
 * @file world_validator.cpp
 * @brief Implementation of @ref validate_world.
 */

#include "entities/world_validator.hpp"
#include <unordered_map>

namespace chronicle {

ValidationReport validate_world(const World &world) {
    ValidationReport report;

    auto error = [&](std::string msg) {
        report.ok = false;
        report.errors.push_back(std::move(msg));
    };

    auto warn = [&](std::string msg) { report.warnings.push_back(std::move(msg)); };

    // -----------------------------------------------------------------------
    // 1. Player's current location must exist
    // -----------------------------------------------------------------------
    if (!world.player.current_location.empty() &&
        !world.locations.contains(world.player.current_location)) {
        error("Player current_location '" + world.player.current_location +
              "' does not exist in world.locations");
    }

    // -----------------------------------------------------------------------
    // 2. Every location exit target must exist
    // -----------------------------------------------------------------------
    for (const auto &[loc_id, loc] : world.locations) {
        for (const auto &[direction, target_id] : loc.exits) {
            if (!world.locations.contains(target_id)) {
                error("Location '" + loc_id + "' exit '" + direction + "' points to '" +
                      target_id + "' which does not exist in world.locations");
            }
        }
    }

    // -----------------------------------------------------------------------
    // 3. Every location item ID must exist in world.items
    // -----------------------------------------------------------------------
    for (const auto &[loc_id, loc] : world.locations) {
        for (const auto &item_id : loc.items) {
            if (!world.items.contains(item_id)) {
                error("Location '" + loc_id + "' references item '" + item_id +
                      "' which does not exist in world.items");
            }
        }
    }

    // -----------------------------------------------------------------------
    // 4. Every location NPC ID must exist in world.npcs
    // -----------------------------------------------------------------------
    for (const auto &[loc_id, loc] : world.locations) {
        for (const auto &npc_id : loc.npcs) {
            if (!world.npcs.contains(npc_id)) {
                error("Location '" + loc_id + "' references NPC '" + npc_id +
                      "' which does not exist in world.npcs");
            }
        }
    }

    // -----------------------------------------------------------------------
    // 5. Every NPC's current_location must exist in world.locations
    // -----------------------------------------------------------------------
    for (const auto &[npc_id, npc] : world.npcs) {
        if (!npc.state.current_location.empty() &&
            !world.locations.contains(npc.state.current_location)) {
            error("NPC '" + npc_id + "' current_location '" + npc.state.current_location +
                  "' does not exist in world.locations");
        }
    }

    // -----------------------------------------------------------------------
    // 6. Every player inventory item ID must exist in world.items
    // -----------------------------------------------------------------------
    for (const auto &item_id : world.player.inventory) {
        if (!world.items.contains(item_id)) {
            error("Player inventory references item '" + item_id +
                  "' which does not exist in world.items");
        }
    }

    // -----------------------------------------------------------------------
    // 7. NPC knowledge fact IDs must exist in world.facts (if facts non-empty)
    // -----------------------------------------------------------------------
    if (!world.facts.empty()) {
        for (const auto &[npc_id, npc] : world.npcs) {
            for (const auto &fact_id : npc.identity.knowledge) {
                if (!world.facts.contains(fact_id)) {
                    error("NPC '" + npc_id + "' knowledge references fact '" + fact_id +
                          "' which does not exist in world.facts");
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // 8. Item ownership uniqueness — each item in at most one container
    // -----------------------------------------------------------------------
    // Map from item_id -> description of where it was first seen
    std::unordered_map<std::string, std::string> item_owner;

    auto track_item = [&](const std::string &item_id, const std::string &owner_desc) {
        auto [it, inserted] = item_owner.emplace(item_id, owner_desc);
        if (!inserted) {
            error("Item '" + item_id + "' has duplicate ownership: found in " + it->second +
                  " and also in " + owner_desc);
        }
    };

    // Player inventory
    for (const auto &item_id : world.player.inventory) {
        track_item(item_id, "player inventory");
    }

    // Location inventories
    for (const auto &[loc_id, loc] : world.locations) {
        for (const auto &item_id : loc.items) {
            track_item(item_id, "location '" + loc_id + "'");
        }
    }

    // NPC inventories
    for (const auto &[npc_id, npc] : world.npcs) {
        for (const auto &item_id : npc.state.inventory) {
            track_item(item_id, "NPC '" + npc_id + "' inventory");
        }
    }

    // -----------------------------------------------------------------------
    // 9. Warning: item has readable=true but no text property
    // -----------------------------------------------------------------------
    for (const auto &[item_id, item] : world.items) {
        auto readable_it = item.properties.find("readable");
        if (readable_it != item.properties.end() && readable_it->second == "true") {
            auto text_it = item.properties.find("text");
            if (text_it == item.properties.end() || text_it->second.empty()) {
                warn("Item '" + item_id +
                     "' has property readable=true but no 'text' property");
            }
        }
    }

    return report;
}

} // namespace chronicle
