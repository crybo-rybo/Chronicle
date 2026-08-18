#include "chronicle/cartridge/validator.hpp"

#include <algorithm>
#include <charconv>
#include <set>

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

bool bool_text(const std::string &text) {
    return text == "true" || text == "false";
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

bool exact_params(const nlohmann::json &params, const std::set<std::string> &required,
                  const std::set<std::string> &optional = {}) {
    if (!params.is_object()) {
        return false;
    }
    for (const auto &key : required) {
        if (!params.contains(key)) {
            return false;
        }
    }
    return std::ranges::all_of(params.items(), [&](const auto &entry) {
        return required.contains(entry.key()) || optional.contains(entry.key());
    });
}

void validate_condition(const WorldState &world, const std::string &event_id,
                        const ConditionData &cond, std::vector<ValidationIssue> &issues) {
    const std::string prefix = "event " + event_id + " condition " + cond.type;
    const auto &args = cond.args;
    if (cond.type == "clock_is") {
        if (args.size() != 1 || !contains(period_names(), args[0])) {
            error(issues, prefix + ": expected one valid period");
        }
    } else if (cond.type == "player_at") {
        if (args.size() != 1 || !world.locations.contains(args[0])) {
            error(issues, prefix + ": bad location");
        }
    } else if (cond.type == "flag_set") {
        if (args.size() != 2 || !known_flag(world, args[0]) || !bool_text(args[1])) {
            error(issues, prefix + ": bad flag");
        }
    } else if (cond.type == "npc_trust_ge") {
        if (args.size() != 2 || !world.npcs.contains(args[0]) || !parses_as_int(args[1])) {
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
        if (!exact_params(action.params, {"npc_id", "location_id"}) ||
            !world.npcs.contains(param_str(action.params, "npc_id")) ||
            !world.locations.contains(param_str(action.params, "location_id"))) {
            error(issues, prefix + ": bad npc/location");
        }
    } else if (action.type == "set_flag") {
        const auto flag_id = param_str(action.params, "flag_id");
        const auto value = action.params.find("value");
        if (!exact_params(action.params, {"flag_id", "value"}) || !known_flag(world, flag_id) ||
            value == action.params.end() || !value->is_boolean()) {
            error(issues, prefix + ": unknown flag " + flag_id);
        }
    } else if (action.type == "spawn_item") {
        if (!exact_params(action.params, {"item_id", "location_id"}) ||
            !world.items.contains(param_str(action.params, "item_id")) ||
            !world.locations.contains(param_str(action.params, "location_id"))) {
            error(issues, prefix + ": bad item/location");
        }
    } else if (action.type == "narrate") {
        if (!exact_params(action.params, {"text"}) || !action.params.at("text").is_string()) {
            error(issues, prefix + ": expected text");
        }
    } else if (action.type == "end_game") {
        if (!exact_params(action.params, {}, {"text"}) ||
            (action.params.contains("text") && !action.params.at("text").is_string())) {
            error(issues, prefix + ": optional text must be a string");
        }
    } else {
        error(issues, prefix + ": unknown action type");
    }
}

} // namespace

bool is_safe_cartridge_id(const std::string &id) {
    const auto is_lower = [](const unsigned char ch) { return ch >= 'a' && ch <= 'z'; };
    const auto is_digit = [](const unsigned char ch) { return ch >= '0' && ch <= '9'; };
    if (id.empty() || id.size() > 64 ||
        (!is_lower(static_cast<unsigned char>(id.front())) &&
         !is_digit(static_cast<unsigned char>(id.front())))) {
        return false;
    }
    return std::ranges::all_of(id, [&](const unsigned char ch) {
        return is_lower(ch) || is_digit(ch) || ch == '_' || ch == '-';
    });
}

std::vector<ValidationIssue> validate_world(const WorldState &world) {
    std::vector<ValidationIssue> issues;

    if (world.manifest.chronicle_schema_version != SCHEMA_VERSION) {
        error(issues, "Unsupported chronicle_schema_version " +
                          std::to_string(world.manifest.chronicle_schema_version) + " (expected " +
                          std::to_string(SCHEMA_VERSION) + ")");
    }
    if (!is_safe_cartridge_id(world.manifest.id)) {
        error(issues,
              "scenario id must match [a-z0-9][a-z0-9_-]{0,63} (safe library directory name)");
    }
    const auto nonblank = [](const std::string &text) {
        return text.find_first_not_of(" \t\r\n") != std::string::npos;
    };
    if (!nonblank(world.manifest.name)) {
        error(issues, "scenario name must be non-empty");
    }
    if (!nonblank(world.manifest.version)) {
        error(issues, "scenario version must be non-empty");
    }
    const auto &config = world.config;
    if (config.temperature < 0.0 || config.temperature > 2.0) {
        error(issues, "temperature must be between 0 and 2");
    }
    if (config.max_response_tokens < 1 || config.max_response_tokens > 16'384) {
        error(issues, "max_response_tokens must be between 1 and 16384");
    }
    if (config.inference_timeout_ms < 100 || config.inference_timeout_ms > 600'000) {
        error(issues, "inference_timeout_ms must be between 100 and 600000");
    }
    if (config.turns_per_period < 1 || config.turns_per_period > 1'000 ||
        config.total_periods < 1 || config.total_periods > 1'000) {
        error(issues, "clock limits must be between 1 and 1000");
    }
    if (config.max_memory_tokens < 1 || config.max_memory_tokens > 1'000'000 ||
        config.max_world_tokens < 1 || config.max_world_tokens > 1'000'000 ||
        config.max_history_tokens < 1 || config.max_history_tokens > 1'000'000) {
        error(issues, "prompt budgets must be between 1 and 1000000");
    }
    if (world.locations.size() > 128 || world.items.size() > 512 || world.npcs.size() > 128 ||
        world.facts.size() > 512 || world.flags.size() > 512 || world.events.size() > 512) {
        error(issues, "cartridge exceeds entity-count limits");
    }
    if (!world.locations.contains(world.player.current_location)) {
        error(issues, "start_location unknown: " + world.player.current_location);
    }

    std::map<std::string, std::string> item_placement;
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
            if (const auto [it, inserted] = item_placement.emplace(item_id, "location " + loc_id);
                !inserted) {
                error(issues, "item " + item_id + " is placed more than once (" + it->second +
                                  " and location " + loc_id + ")");
            }
        }
        for (const auto &npc_id : loc.npcs) {
            const auto npc = world.npcs.find(npc_id);
            if (npc == world.npcs.end() || npc->second.state.current_location != loc_id) {
                error(issues, "location " + loc_id + ": inconsistent npc " + npc_id);
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
            if (const auto [it, inserted] = item_placement.emplace(item_id, "npc " + npc_id);
                !inserted) {
                error(issues, "item " + item_id + " is placed more than once (" + it->second +
                                  " and npc " + npc_id + ")");
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
