/**
 * @file event.cpp
 * @brief Implementation of event system serialisation and string conversion utilities.
 */

#include "entities/event.hpp"
#include <stdexcept>

namespace chronicle {

std::string condition_type_to_string(ConditionType t) {
    switch (t) {
    case ConditionType::ClockIs:
        return "clock_is";
    case ConditionType::PlayerAt:
        return "player_at";
    case ConditionType::FlagSet:
        return "flag_set";
    case ConditionType::NpcTrustGe:
        return "npc_trust_ge";
    case ConditionType::NpcAt:
        return "npc_at";
    case ConditionType::ItemInPlayerInv:
        return "item_in_player_inv";
    case ConditionType::TurnGe:
        return "turn_ge";
    }
    // Unreachable, but suppress compiler warning
    return "clock_is";
}

ConditionType string_to_condition_type(const std::string &s) {
    if (s == "clock_is")
        return ConditionType::ClockIs;
    if (s == "player_at")
        return ConditionType::PlayerAt;
    if (s == "flag_set")
        return ConditionType::FlagSet;
    if (s == "npc_trust_ge")
        return ConditionType::NpcTrustGe;
    if (s == "npc_at")
        return ConditionType::NpcAt;
    if (s == "item_in_player_inv")
        return ConditionType::ItemInPlayerInv;
    if (s == "turn_ge")
        return ConditionType::TurnGe;
    throw std::invalid_argument("Unknown condition type: " + s);
}

void to_json(nlohmann::json &j, const ConditionType &t) {
    j = condition_type_to_string(t);
}

void from_json(const nlohmann::json &j, ConditionType &t) {
    t = string_to_condition_type(j.get<std::string>());
}

void to_json(nlohmann::json &j, const Condition &c) {
    j = nlohmann::json{{"type", c.type}, {"args", c.args}};
}

void from_json(const nlohmann::json &j, Condition &c) {
    j.at("type").get_to(c.type);
    c.args = j.value("args", std::vector<std::string>{});
}

void to_json(nlohmann::json &j, const EventAction &a) {
    j = nlohmann::json{{"type", a.type}, {"params", a.params}};
}

void from_json(const nlohmann::json &j, EventAction &a) {
    a.type = j.at("type").get<std::string>();
    a.params = j.value("params", std::map<std::string, std::string>{});
}

void to_json(nlohmann::json &j, const EventTrigger &t) {
    j = nlohmann::json{{"id", t.id},
                       {"conditions", t.conditions},
                       {"actions", t.actions},
                       {"once", t.once},
                       {"fired", t.fired}};
}

void from_json(const nlohmann::json &j, EventTrigger &t) {
    t.id = j.at("id").get<std::string>();
    t.conditions = j.value("conditions", std::vector<Condition>{});
    t.actions = j.value("actions", std::vector<EventAction>{});
    t.once = j.value("once", true);
    t.fired = j.value("fired", false);
}

} // namespace chronicle
