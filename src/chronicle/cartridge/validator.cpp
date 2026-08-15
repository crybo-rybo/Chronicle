#include "chronicle/cartridge/validator.hpp"

#include <algorithm>
#include <charconv>

#include "chronicle/cartridge/loader.hpp"

namespace chronicle {

namespace {

bool contains(const std::vector<std::string> &haystack, const std::string &needle) {
    return std::ranges::find(haystack, needle) != haystack.end();
}

bool parses_as_int(const std::string &text) {
    int value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc{} && ptr == text.data() + text.size();
}

bool known_flag(const WorldState &world, const std::string &flag_id) {
    return world.flags.contains(flag_id) || world.flag_meta.contains(flag_id);
}

void error(std::vector<ValidationIssue> &issues, std::string message) {
    issues.push_back({.message = std::move(message), .level = IssueLevel::error});
}

void warning(std::vector<ValidationIssue> &issues, std::string message) {
    issues.push_back({.message = std::move(message), .level = IssueLevel::warning});
}

std::string param_str(const nlohmann::json &params, const char *key) {
    const auto it = params.find(key);
    if (it == params.end() || !it->is_string()) {
        return "";
    }
    return it->get<std::string>();
}

void validate_condition(const WorldState &world, const std::string &event_id,
                        const ConditionData &cond, std::vector<ValidationIssue> &issues) {
    const std::string prefix = "event " + event_id + " condition " + cond.type;
    const auto &args = cond.args;
    if (cond.type == "clock_is") {
        if (args.size() != 1) {
            error(issues, prefix + ": expected 1 arg");
        }
    } else if (cond.type == "player_at") {
        if (args.size() != 1 || !world.locations.contains(args[0])) {
            error(issues, prefix + ": bad location");
        }
    } else if (cond.type == "flag_set") {
        if (args.size() != 2 || !known_flag(world, args[0])) {
            error(issues, prefix + ": bad flag");
        }
    } else if (cond.type == "npc_trust_ge") {
        if (args.size() != 2 || !world.npcs.contains(args[0])) {
            error(issues, prefix + ": bad npc/threshold");
        }
    } else if (cond.type == "npc_at") {
        if (args.size() != 2 || !world.npcs.contains(args[0]) ||
            !world.locations.contains(args[1])) {
            error(issues, prefix + ": bad npc/location");
        }
    } else if (cond.type == "item_in_player_inv") {
        if (args.size() != 1 || !world.items.contains(args[0])) {
            error(issues, prefix + ": bad item");
        }
    } else if (cond.type == "turn_ge") {
        if (args.size() != 1) {
            error(issues, prefix + ": expected turn count");
        } else if (!parses_as_int(args[0])) {
            error(issues, prefix + ": turn count not an int");
        }
    } else {
        error(issues, prefix + ": unknown condition type");
    }
}

void validate_event_action(const WorldState &world, const std::string &event_id,
                           const EventActionData &action, std::vector<ValidationIssue> &issues) {
    const std::string prefix = "event " + event_id + " action " + action.type;
    if (action.type == "move_npc") {
        if (!world.npcs.contains(param_str(action.params, "npc_id")) ||
            !world.locations.contains(param_str(action.params, "location_id"))) {
            error(issues, prefix + ": bad npc/location");
        }
    } else if (action.type == "set_flag") {
        const auto flag_id = param_str(action.params, "flag_id");
        if (!known_flag(world, flag_id)) {
            error(issues, prefix + ": unknown flag " + flag_id);
        }
    } else if (action.type == "spawn_item") {
        if (!world.items.contains(param_str(action.params, "item_id")) ||
            !world.locations.contains(param_str(action.params, "location_id"))) {
            error(issues, prefix + ": bad item/location");
        }
    } else if (action.type == "narrate" || action.type == "end_game") {
        return;
    } else {
        error(issues, prefix + ": unknown action type");
    }
}

} // namespace

std::vector<ValidationIssue> validate_world(const WorldState &world) {
    std::vector<ValidationIssue> issues;

    if (world.manifest.chronicle_schema_version != SCHEMA_VERSION) {
        error(issues, "Unsupported chronicle_schema_version " +
                          std::to_string(world.manifest.chronicle_schema_version) + " (expected " +
                          std::to_string(SCHEMA_VERSION) + ")");
    }
    if (world.manifest.id.find_first_not_of(" \t\r\n") == std::string::npos) {
        error(issues, "scenario id must be non-empty");
    }
    if (!world.locations.contains(world.player.current_location)) {
        error(issues, "start_location unknown: " + world.player.current_location);
    }

    for (const auto &[loc_id, loc] : world.locations) {
        for (const auto &[direction, dest] : loc.exits) {
            if (!world.locations.contains(dest)) {
                error(issues, "location " + loc_id + ": exit " + direction + " -> unknown " + dest);
            }
        }
        for (const auto &item_id : loc.items) {
            if (!world.items.contains(item_id)) {
                error(issues, "location " + loc_id + ": unknown item " + item_id);
            }
        }
    }

    for (const auto &[item_id, item] : world.items) {
        if (!world.item_owners.contains(item_id)) {
            warning(issues, "item " + item_id + " is not placed anywhere");
        }
    }

    for (const auto &[npc_id, npc] : world.npcs) {
        if (!world.locations.contains(npc.state.current_location)) {
            error(issues, "npc " + npc_id + ": unknown location " + npc.state.current_location);
        }
        if (!contains(valid_moods(), npc.state.mood)) {
            error(issues, "npc " + npc_id + ": invalid mood " + npc.state.mood);
        }
        for (const auto &fact_id : npc.identity.knowledge) {
            if (!world.facts.contains(fact_id)) {
                error(issues, "npc " + npc_id + ": unknown knowledge fact " + fact_id);
            }
        }
        for (const auto &item_id : npc.state.inventory) {
            if (!world.items.contains(item_id)) {
                error(issues, "npc " + npc_id + ": unknown inventory item " + item_id);
            }
        }
        const auto &policy = npc.identity.tool_policy;
        for (const auto &tool : policy.allowed_tools) {
            if (!contains(npc_tool_names(), tool)) {
                error(issues, "npc " + npc_id + ": unknown tool " + tool);
            }
        }
        for (const auto &item_id : policy.allowed_items) {
            if (!world.items.contains(item_id)) {
                error(issues, "npc " + npc_id + ": allowed_items unknown " + item_id);
            }
        }
        for (const auto &fact_id : policy.allowed_facts) {
            if (!world.facts.contains(fact_id)) {
                error(issues, "npc " + npc_id + ": allowed_facts unknown " + fact_id);
            }
        }
        for (const auto &flag_id : policy.allowed_flags) {
            if (!known_flag(world, flag_id)) {
                error(issues, "npc " + npc_id + ": allowed_flags unknown " + flag_id);
            }
        }
        for (const auto &loc_id : policy.allowed_locations) {
            if (!world.locations.contains(loc_id)) {
                error(issues, "npc " + npc_id + ": allowed_locations unknown " + loc_id);
            }
        }
    }

    for (const auto &[event_id, event] : world.events) {
        for (const auto &cond : event.conditions) {
            validate_condition(world, event_id, cond, issues);
        }
        for (const auto &action : event.actions) {
            validate_event_action(world, event_id, action, issues);
        }
    }

    return issues;
}

std::vector<ValidationIssue> validate_package(const std::filesystem::path &dir) {
    try {
        return validate_world(load_package(dir));
    } catch (const std::exception &exc) {
        return {{.message = exc.what(), .level = IssueLevel::error}};
    }
}

bool has_errors(const std::vector<ValidationIssue> &issues) {
    return std::ranges::any_of(issues,
                               [](const auto &issue) { return issue.level == IssueLevel::error; });
}

} // namespace chronicle
