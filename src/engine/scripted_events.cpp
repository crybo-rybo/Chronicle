/**
 * @file scripted_events.cpp
 * @brief Implementation of scripted event evaluation.
 */

#include "engine/scripted_events.hpp"
#include "common/parse_utils.hpp"
#include "diagnostics/logger.hpp"
#include "entities/clock.hpp"
#include <algorithm>

namespace chronicle {

namespace {

std::string generic_ending_text() { return "The scenario has reached its conclusion."; }

bool event_condition_matches(const World &world, const Condition &condition) {
    switch (condition.type) {
    case ConditionType::ClockIs:
        if (condition.args.size() != 1) {
            return false;
        }
        try {
            return world.clock.period == string_to_time_period(condition.args[0]);
        } catch (const std::exception &) {
            return false;
        }
    case ConditionType::PlayerAt:
        return condition.args.size() == 1 && world.player.current_location == condition.args[0];
    case ConditionType::FlagSet: {
        if (condition.args.size() != 2) {
            return false;
        }
        auto flag_it = world.flags.find(condition.args[0]);
        auto expected = parse_bool(condition.args[1]);
        return flag_it != world.flags.end() && expected && flag_it->second == *expected;
    }
    case ConditionType::NpcTrustGe: {
        if (condition.args.size() != 2) {
            return false;
        }
        auto npc_it = world.npcs.find(condition.args[0]);
        auto threshold = parse_int(condition.args[1]);
        return npc_it != world.npcs.end() && threshold &&
               npc_it->second.state.trust_toward_player >= *threshold;
    }
    case ConditionType::NpcAt: {
        if (condition.args.size() != 2) {
            return false;
        }
        auto npc_it = world.npcs.find(condition.args[0]);
        return npc_it != world.npcs.end() &&
               npc_it->second.state.current_location == condition.args[1];
    }
    case ConditionType::ItemInPlayerInv:
        return condition.args.size() == 1 &&
               std::ranges::contains(world.player.inventory, condition.args[0]);
    case ConditionType::TurnGe: {
        if (condition.args.size() != 1) {
            return false;
        }
        auto threshold = parse_int(condition.args[0]);
        return threshold && world.total_turns_elapsed >= *threshold;
    }
    }
    return false;
}

bool event_conditions_match(const World &world, const EventTrigger &event) {
    return std::ranges::all_of(event.conditions, [&](const Condition &condition) {
        return event_condition_matches(world, condition);
    });
}

} // namespace

ScriptedEventResult evaluate_scripted_events(World &world, ScriptedEventSink &sink) {
    ScriptedEventResult result;

    for (auto &event : world.events) {
        if (event.once && event.fired) {
            continue;
        }
        if (!event_conditions_match(world, event)) {
            continue;
        }

        logging::write(logging::Level::Info, "events", "triggering event=" + event.id);

        bool ended_game = false;
        for (const auto &action : event.actions) {
            if (action.type == "move_npc") {
                auto npc_id = param_value(action.params, "npc_id");
                auto location_id = param_value(action.params, "location_id");
                if (!npc_id || !location_id) {
                    logging::write(logging::Level::Warning, "events",
                                   "skipping malformed move_npc action event=" + event.id);
                    continue;
                }
                sink.enqueue_mutation(MutationRequest{
                    .type = MutationRequest::Type::MoveNpc,
                    .source = MutationRequest::Source::System,
                    .actor_id = *npc_id,
                    .params = {{"location_id", *location_id}},
                });
            } else if (action.type == "set_flag") {
                auto flag_id = param_value(action.params, "flag_id");
                auto value = param_value(action.params, "value");
                if (!flag_id || !value) {
                    logging::write(logging::Level::Warning, "events",
                                   "skipping malformed set_flag action event=" + event.id);
                    continue;
                }
                sink.enqueue_mutation(MutationRequest{
                    .type = MutationRequest::Type::SetFlag,
                    .source = MutationRequest::Source::System,
                    .actor_id = event.id,
                    .params = {{"flag_id", *flag_id}, {"value", *value}},
                });
            } else if (action.type == "spawn_item") {
                auto item_id = param_value(action.params, "item_id");
                auto location_id = param_value(action.params, "location_id");
                if (!item_id || !location_id) {
                    logging::write(logging::Level::Warning, "events",
                                   "skipping malformed spawn_item action event=" + event.id);
                    continue;
                }
                sink.enqueue_mutation(MutationRequest{
                    .type = MutationRequest::Type::SpawnItem,
                    .source = MutationRequest::Source::System,
                    .actor_id = event.id,
                    .params = {{"item_id", *item_id}, {"location_id", *location_id}},
                });
            } else if (action.type == "narrate") {
                if (auto text = param_value(action.params, "text")) {
                    sink.narrate(*text);
                }
            } else if (action.type == "end_game") {
                sink.end_game(param_value(action.params, "text").value_or(generic_ending_text()));
                ended_game = true;
                break;
            } else {
                logging::write(logging::Level::Warning, "events",
                               "skipping unknown action type=" + action.type +
                                   " event=" + event.id);
            }
        }

        if (event.once) {
            event.fired = true;
        }
        if (ended_game) {
            result.ended_game = true;
            break;
        }
    }

    return result;
}

} // namespace chronicle
