#include "chronicle/game/cartridge_game.hpp"

#include <algorithm>

#include "chronicle/game/text_utils.hpp"

namespace chronicle {

namespace {

using game_detail::lower;
using game_detail::parse_int;

} // namespace

// --- scripted events -----------------------------------------------------

// Events fire in id order. One fires only if its conditions held before any event
// of this pass ran and still hold when its place comes: it never fires on a state
// that an earlier event undid, nor sooner than the turn after the one that set it up.
GameEvents CartridgeGame::evaluate_events(const bool turn_passed) {
    std::vector<std::string> ready;
    for (const auto &[event_id, trigger] : world_.events) {
        // A repeating event is checked once per turn of the clock, not per line typed.
        const bool due = trigger.once ? !trigger.fired : turn_passed;
        if (due && conditions_met(trigger)) {
            ready.push_back(event_id);
        }
    }

    GameEvents events;
    for (const auto &event_id : ready) {
        if (!conditions_met(world_.events.at(event_id))) {
            continue;
        }
        if (auto marked = submit_world_action(actions::MarkEventFired{.event_id = event_id});
            !marked) {
            events.push_back({EventKind::warning, marked.error().reason});
            continue;
        }
        for (const auto &action : world_.events.at(event_id).actions) {
            auto applied = apply_event_action(action);
            events.insert(events.end(), applied.begin(), applied.end());
            if (phase_ == GamePhase::game_over) {
                return events; // One ending, and nothing after it.
            }
        }
    }
    return events;
}

bool CartridgeGame::conditions_met(const EventTriggerData &trigger) const {
    return std::ranges::all_of(trigger.conditions,
                               [&](const ConditionData &cond) { return condition_met(cond); });
}

bool CartridgeGame::condition_met(const ConditionData &cond) const {
    const auto &args = cond.args;
    if (cond.type == "clock_is") {
        return !args.empty() && world_.clock.period_name() == args[0];
    }
    if (cond.type == "player_at") {
        return !args.empty() && world_.player.current_location == args[0];
    }
    if (cond.type == "flag_set") {
        if (args.size() < 2) {
            return false;
        }
        const std::string want_text = lower(args[1]);
        const bool want = want_text == "1" || want_text == "true" || want_text == "yes";
        const auto it = world_.flags.find(args[0]);
        const bool value = it != world_.flags.end() && it->second;
        return value == want;
    }
    if (cond.type == "npc_trust_ge") {
        if (args.size() < 2) {
            return false;
        }
        const auto npc = world_.npcs.find(args[0]);
        const auto threshold = parse_int(args[1]);
        return npc != world_.npcs.end() && threshold &&
               npc->second.state.trust_toward_player >= *threshold;
    }
    if (cond.type == "npc_at") {
        if (args.size() < 2) {
            return false;
        }
        const auto npc = world_.npcs.find(args[0]);
        return npc != world_.npcs.end() && npc->second.state.current_location == args[1];
    }
    if (cond.type == "item_in_player_inv") {
        return !args.empty() && item_is_at(world_, args[0], ItemHolder::player);
    }
    if (cond.type == "turn_ge") {
        if (args.empty()) {
            return false;
        }
        const auto threshold = parse_int(args[0]);
        return threshold && world_.clock.turns_elapsed >= *threshold;
    }
    return false;
}

GameEvents CartridgeGame::apply_event_action(const EventActionData &action) {
    const auto &params = action.params;
    const auto str_param = [&](const char *key) -> std::string {
        const auto it = params.find(key);
        return it != params.end() && it->is_string() ? it->get<std::string>() : "";
    };

    if (action.type == "narrate") {
        return {{EventKind::narration, str_param("text")}};
    }
    if (action.type == "end_game") {
        if (auto ended = submit_world_action(actions::EndGame{}); !ended) {
            return {{EventKind::warning, ended.error().reason}};
        }
        const std::string text = str_param("text");
        return {{EventKind::ending, text.empty() ? "The scenario ends." : text}};
    }
    if (action.type == "move_npc") {
        const auto npc_it = world_.npcs.find(str_param("npc_id"));
        const auto loc_it = world_.locations.find(str_param("location_id"));
        if (npc_it == world_.npcs.end() || loc_it == world_.locations.end()) {
            return {};
        }
        if (auto moved = submit_world_action(actions::MoveNpc{
                .npc_id = npc_it->first, .destination = loc_it->first, .significant = false});
            !moved) {
            return {{EventKind::warning, moved.error().reason}};
        }
        return {{EventKind::narration,
                 npc_it->second.identity.name + " moves to " + loc_it->second.name + "."}};
    }
    if (action.type == "set_flag") {
        const std::string flag_id = str_param("flag_id");
        if (flag_id.empty()) {
            return {};
        }
        const auto value_it = params.find("value");
        if (value_it == params.end() || !value_it->is_boolean()) {
            return {{EventKind::warning, "Event set_flag value is not boolean"}};
        }
        if (auto set = submit_world_action(actions::SetFlag{
                .flag_id = flag_id, .value = value_it->get<bool>(), .significant = false});
            !set) {
            return {{EventKind::warning, set.error().reason}};
        }
        return {};
    }
    if (action.type == "spawn_item") {
        const std::string item_id = str_param("item_id");
        const auto loc_it = world_.locations.find(str_param("location_id"));
        if (item_id.empty() || loc_it == world_.locations.end()) {
            return {};
        }
        // What the player already carries, or what is already there, does not appear.
        if (item_is_at(world_, item_id, ItemHolder::player) ||
            item_is_at(world_, item_id, ItemHolder::location, loc_it->first)) {
            return {};
        }
        if (auto moved = submit_world_action(actions::RelocateItem{
                .item_id = item_id,
                .destination = ItemPosition{.holder = ItemHolder::location, .id = loc_it->first},
                .significant = false,
            });
            !moved) {
            return {{EventKind::warning, moved.error().reason}};
        }
        return {{EventKind::narration, "Something new appears in " + loc_it->second.name + "."}};
    }
    return {};
}

} // namespace chronicle
