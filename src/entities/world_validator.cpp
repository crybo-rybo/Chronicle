/**
 * @file world_validator.cpp
 * @brief Implementation of @ref validate_world.
 */

#include "entities/world_validator.hpp"
#include "engine/parse_utils.hpp"
#include "entities/clock.hpp"
#include <algorithm>
#include <unordered_map>

namespace chronicle {

namespace {

std::string condition_name(ConditionType type) {
    return condition_type_to_string(type);
}

std::string condition_context(const EventTrigger &event, std::size_t index, ConditionType type) {
    return "Event '" + event.id + "' condition #" + std::to_string(index) + " (" +
           condition_name(type) + ")";
}

std::string action_context(const EventTrigger &event, std::size_t index, const std::string &type) {
    return "Event '" + event.id + "' action #" + std::to_string(index) + " (" + type + ")";
}

} // namespace

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
                error("Location '" + loc_id + "' exit '" + direction + "' points to '" + target_id +
                      "' which does not exist in world.locations");
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
    // 7. NPC knowledge fact IDs must exist in world.facts
    // -----------------------------------------------------------------------
    for (const auto &[npc_id, npc] : world.npcs) {
        for (const auto &fact_id : npc.identity.knowledge) {
            if (!world.facts.contains(fact_id)) {
                error("NPC '" + npc_id + "' knowledge references fact '" + fact_id +
                      "' which does not exist in world.facts");
            }
        }

        for (const auto &tool : npc.identity.tool_policy.allowed_tools) {
            if (!is_valid_npc_tool(tool)) {
                error("NPC '" + npc_id + "' tool_policy references unknown tool '" + tool + "'");
            }
        }
        for (const auto &item_id : npc.identity.tool_policy.allowed_items) {
            if (!world.items.contains(item_id)) {
                error("NPC '" + npc_id + "' tool_policy allowed_items references missing item '" +
                      item_id + "'");
            }
        }
        for (const auto &fact_id : npc.identity.tool_policy.allowed_facts) {
            if (!world.facts.contains(fact_id)) {
                error("NPC '" + npc_id + "' tool_policy allowed_facts references missing fact '" +
                      fact_id + "'");
            }
        }
        for (const auto &flag_id : npc.identity.tool_policy.allowed_flags) {
            if (!world.flags.contains(flag_id)) {
                error("NPC '" + npc_id + "' tool_policy allowed_flags references missing flag '" +
                      flag_id + "'");
            }
        }
        for (const auto &location_id : npc.identity.tool_policy.allowed_locations) {
            if (!world.locations.contains(location_id)) {
                error("NPC '" + npc_id +
                      "' tool_policy allowed_locations references missing location '" +
                      location_id + "'");
            }
        }
    }

    // -----------------------------------------------------------------------
    // 8. Event condition/action semantic validation
    // -----------------------------------------------------------------------
    auto require_arg_count = [&](const EventTrigger &event, std::size_t index,
                                 const Condition &condition, std::size_t expected) {
        if (condition.args.size() != expected) {
            error(condition_context(event, index, condition.type) + " requires " +
                  std::to_string(expected) + " arg(s), got " +
                  std::to_string(condition.args.size()));
            return false;
        }
        return true;
    };

    auto require_param = [&](const EventTrigger &event, std::size_t index,
                             const EventAction &action,
                             const std::string &key) -> std::optional<std::string> {
        auto it = action.params.find(key);
        if (it == action.params.end() || it->second.empty()) {
            error(action_context(event, index, action.type) + " requires non-empty '" + key +
                  "' param");
            return std::nullopt;
        }
        return it->second;
    };

    for (const auto &event : world.events) {
        for (std::size_t i = 0; i < event.conditions.size(); ++i) {
            const auto &condition = event.conditions[i];
            switch (condition.type) {
            case ConditionType::ClockIs:
                if (require_arg_count(event, i, condition, 1)) {
                    try {
                        (void)string_to_time_period(condition.args[0]);
                    } catch (const std::exception &) {
                        error(condition_context(event, i, condition.type) +
                              " references unknown time period '" + condition.args[0] + "'");
                    }
                }
                break;
            case ConditionType::PlayerAt:
                if (require_arg_count(event, i, condition, 1) &&
                    !world.locations.contains(condition.args[0])) {
                    error(condition_context(event, i, condition.type) +
                          " references missing location '" + condition.args[0] + "'");
                }
                break;
            case ConditionType::FlagSet:
                if (require_arg_count(event, i, condition, 2)) {
                    if (!world.flags.contains(condition.args[0])) {
                        error(condition_context(event, i, condition.type) +
                              " references undeclared flag '" + condition.args[0] + "'");
                    }
                    if (!parse_bool(condition.args[1])) {
                        error(condition_context(event, i, condition.type) +
                              " value must be a boolean string, got '" + condition.args[1] + "'");
                    }
                }
                break;
            case ConditionType::NpcTrustGe:
                if (require_arg_count(event, i, condition, 2)) {
                    if (!world.npcs.contains(condition.args[0])) {
                        error(condition_context(event, i, condition.type) +
                              " references missing NPC '" + condition.args[0] + "'");
                    }
                    if (!parse_int(condition.args[1])) {
                        error(condition_context(event, i, condition.type) +
                              " threshold must be an integer, got '" + condition.args[1] + "'");
                    }
                }
                break;
            case ConditionType::NpcAt:
                if (require_arg_count(event, i, condition, 2)) {
                    if (!world.npcs.contains(condition.args[0])) {
                        error(condition_context(event, i, condition.type) +
                              " references missing NPC '" + condition.args[0] + "'");
                    }
                    if (!world.locations.contains(condition.args[1])) {
                        error(condition_context(event, i, condition.type) +
                              " references missing location '" + condition.args[1] + "'");
                    }
                }
                break;
            case ConditionType::ItemInPlayerInv:
                if (require_arg_count(event, i, condition, 1) &&
                    !world.items.contains(condition.args[0])) {
                    error(condition_context(event, i, condition.type) +
                          " references missing item '" + condition.args[0] + "'");
                }
                break;
            case ConditionType::TurnGe:
                if (require_arg_count(event, i, condition, 1)) {
                    auto turns = parse_int(condition.args[0]);
                    if (!turns || *turns < 0) {
                        error(condition_context(event, i, condition.type) +
                              " threshold must be a non-negative integer, got '" +
                              condition.args[0] + "'");
                    }
                }
                break;
            }
        }

        for (std::size_t i = 0; i < event.actions.size(); ++i) {
            const auto &action = event.actions[i];
            if (action.type == "move_npc") {
                auto npc_id = require_param(event, i, action, "npc_id");
                auto location_id = require_param(event, i, action, "location_id");
                if (npc_id && !world.npcs.contains(*npc_id)) {
                    error(action_context(event, i, action.type) + " references missing NPC '" +
                          *npc_id + "'");
                }
                if (location_id && !world.locations.contains(*location_id)) {
                    error(action_context(event, i, action.type) + " references missing location '" +
                          *location_id + "'");
                }
            } else if (action.type == "set_flag") {
                auto flag_id = require_param(event, i, action, "flag_id");
                auto value = require_param(event, i, action, "value");
                if (flag_id && !world.flags.contains(*flag_id)) {
                    error(action_context(event, i, action.type) + " references undeclared flag '" +
                          *flag_id + "'");
                }
                if (value && !parse_bool(*value)) {
                    error(action_context(event, i, action.type) +
                          " value must be a boolean string, got '" + *value + "'");
                }
            } else if (action.type == "spawn_item") {
                auto item_id = require_param(event, i, action, "item_id");
                auto location_id = require_param(event, i, action, "location_id");
                if (item_id && !world.items.contains(*item_id)) {
                    error(action_context(event, i, action.type) + " references missing item '" +
                          *item_id + "'");
                }
                if (location_id && !world.locations.contains(*location_id)) {
                    error(action_context(event, i, action.type) + " references missing location '" +
                          *location_id + "'");
                }
            } else if (action.type == "narrate") {
                (void)require_param(event, i, action, "text");
            } else if (action.type == "end_game") {
                // Control-only action; optional text is used as final narration.
            } else {
                error(action_context(event, i, action.type) + " has unknown action type '" +
                      action.type + "'");
            }
        }
    }

    // -----------------------------------------------------------------------
    // 9. Item ownership uniqueness — each item in at most one container
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
    // 10. Warning: item has readable=true but no text property
    // -----------------------------------------------------------------------
    for (const auto &[item_id, item] : world.items) {
        auto readable_it = item.properties.find("readable");
        if (readable_it != item.properties.end() && readable_it->second == "true") {
            auto text_it = item.properties.find("text");
            if (text_it == item.properties.end() || text_it->second.empty()) {
                warn("Item '" + item_id + "' has property readable=true but no 'text' property");
            }
        }
    }

    return report;
}

} // namespace chronicle
