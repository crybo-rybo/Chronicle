#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace chronicle {

/// Types of conditions that can be evaluated against World state.
/// NOTE: Relies on sequential integer values — do not reorder or assign explicit values.
enum class ConditionType {
    ClockIs,         ///< clock.period matches a named TimePeriod
    PlayerAt,        ///< player.current_location == arg
    FlagSet,         ///< flags[arg[0]] == (arg[1] == "true")
    NpcTrustGe,      ///< npc[arg[0]].state.trust_toward_player >= stoi(arg[1])
    NpcAt,           ///< npc[arg[0]].state.current_location == arg[1]
    ItemInPlayerInv, ///< player.inventory contains arg[0]
    TurnGe           ///< total_turns_elapsed >= stoi(arg[0])
};

std::string condition_type_to_string(ConditionType t);
ConditionType string_to_condition_type(const std::string &s);

void to_json(nlohmann::json &j, const ConditionType &t);
void from_json(const nlohmann::json &j, ConditionType &t);

/// A single predicate evaluated against World state. All conditions in a trigger use AND semantics.
struct Condition {
    ConditionType type;
    std::vector<std::string> args; ///< Polymorphic arguments (count/meaning depend on type)
};

void to_json(nlohmann::json &j, const Condition &c);
void from_json(const nlohmann::json &j, Condition &c);

/// An action to execute when a trigger fires. Executed by GameEngine, not EventSystem.
struct EventAction {
    std::string type; ///< "move_npc", "set_flag", "spawn_item", "narrate", "end_game"
    std::map<std::string, std::string> params; ///< Action-specific parameters
};

void to_json(nlohmann::json &j, const EventAction &a);
void from_json(const nlohmann::json &j, EventAction &a);

/// A scripted trigger: fires when ALL conditions are satisfied, executes all actions.
struct EventTrigger {
    std::string id;                    ///< Set from JSON map key by world loader
    std::vector<Condition> conditions; ///< AND semantics — all must be true
    std::vector<EventAction> actions;
    bool once = true;   ///< If true, trigger is disabled after first firing
    bool fired = false; ///< Runtime state — true after the trigger has fired
};

void to_json(nlohmann::json &j, const EventTrigger &t);
void from_json(const nlohmann::json &j, EventTrigger &t);

} // namespace chronicle
