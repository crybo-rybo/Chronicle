/**
 * @file event.hpp
 * @brief Scripted event trigger system for Chronicle.
 *
 * @details The event system provides deterministic, LLM-independent scripted
 * triggers that fire when world state meets defined conditions.  Triggers are
 * evaluated once per turn, after all pending mutations have been applied,
 * allowing them to respond to state changes that just occurred.
 *
 * Each @ref EventTrigger bundles one or more @ref Condition predicates (AND
 * semantics) with one or more @ref EventAction descriptors.  When all
 * conditions are satisfied, @ref GameEngine executes the actions.  Optional
 * @c once semantics prevent a trigger from firing more than once.
 *
 * Condition arguments are stored as plain strings to keep the data format
 * flexible; the engine interprets them according to the @ref ConditionType.
 */

#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace chronicle {

/// @brief Enumeration of predicates that can be tested against world state.
///
/// @details Each value corresponds to a specific check the event system
/// can perform.  The string representation (used in JSON data files) is
/// defined by @ref condition_type_to_string and @ref string_to_condition_type.
enum class ConditionType {
    ClockIs,         ///< @c clock.period matches a named @ref TimePeriod value.
    PlayerAt,        ///< @c player.current_location == args[0].
    FlagSet,         ///< @c flags[args[0]] == (args[1] == @c "true").
    NpcTrustGe,      ///< @c npc[args[0]].state.trust_toward_player >= stoi(args[1]).
    NpcAt,           ///< @c npc[args[0]].state.current_location == args[1].
    ItemInPlayerInv, ///< @c player.inventory contains args[0].
    TurnGe           ///< @c total_turns_elapsed >= stoi(args[0]).
};

/// @brief Convert a @ref ConditionType to its JSON string representation.
/// @param t The condition type to convert.
/// @return Lowercase snake_case string, e.g. @c "clock_is".
std::string condition_type_to_string(ConditionType t);

/// @brief Parse a condition type string into the corresponding enum value.
/// @param s The string to parse.  Case-sensitive.
/// @return The matching @ref ConditionType.
/// @throws std::invalid_argument if @p s is not a recognised condition type.
ConditionType string_to_condition_type(const std::string &s);

/// @cond INTERNAL
void to_json(nlohmann::json &j, const ConditionType &t);
void from_json(const nlohmann::json &j, ConditionType &t);
/// @endcond

/// @brief A single predicate evaluated against the current world state.
///
/// @details All conditions within a given @ref EventTrigger use AND semantics —
/// every condition must be @c true for the trigger to fire.  The meaning and
/// required count of @c args depends on the @c type; see @ref ConditionType.
struct Condition {
    ConditionType type;            ///< Which predicate to evaluate.
    std::vector<std::string> args; ///< Polymorphic arguments (count and meaning depend on @c type).
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const Condition &c);
void from_json(const nlohmann::json &j, Condition &c);
/// @endcond

/// @brief Describes an action to perform when a trigger fires.
///
/// @details Actions are interpreted and executed by @ref GameEngine, not by
/// the event system itself.  The event system only evaluates conditions and
/// returns the list of actions that should run.
///
/// Common action types: @c "move_npc", @c "set_flag", @c "spawn_item",
/// @c "narrate", @c "end_game".  Parameters are action-type-specific.
struct EventAction {
    /// Identifies which action to perform (e.g. @c "move_npc").
    std::string type;

    /// @brief Action-specific named parameters.
    ///
    /// For example, a @c "move_npc" action uses keys @c "npc_id" and
    /// @c "location_id".
    std::map<std::string, std::string> params;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const EventAction &a);
void from_json(const nlohmann::json &j, EventAction &a);
/// @endcond

/// @brief A scripted trigger that fires when all conditions are satisfied.
///
/// @details Triggers are loaded from @c events.json at startup and stored in
/// @ref World::events.  Each turn, after mutations are applied, @ref GameEngine
/// iterates over all unfired triggers and evaluates their conditions.  When a
/// trigger fires, its actions are applied and, if @c once is @c true, the
/// trigger is disabled so it cannot fire again.
struct EventTrigger {
    /// @brief Unique identifier for this trigger.
    ///
    /// Set from the JSON map key by the world loader.
    std::string id;

    /// @brief Conditions that must all be true for this trigger to fire.
    ///
    /// AND semantics: every condition in this list must evaluate to @c true.
    std::vector<Condition> conditions;

    /// Actions to execute when all conditions are satisfied.
    std::vector<EventAction> actions;

    /// If @c true, the trigger is disabled after firing once.  Defaults to @c true.
    bool once = true;

    /// @brief Runtime state indicating whether this trigger has already fired.
    ///
    /// Persisted in save files so one-shot triggers do not re-fire after load.
    bool fired = false;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const EventTrigger &t);
void from_json(const nlohmann::json &j, EventTrigger &t);
/// @endcond

} // namespace chronicle
