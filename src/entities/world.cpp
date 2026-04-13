/**
 * @file world.cpp
 * @brief Implementation of @ref World JSON serialisation.
 */

#include "entities/world.hpp"

namespace chronicle {

void to_json(nlohmann::json &j, const World &w) {
    j = nlohmann::json{
        {"clock", w.clock},   {"locations", w.locations},
        {"npcs", w.npcs},     {"items", w.items},
        {"player", w.player}, {"flags", w.flags},
        {"events", w.events}, {"total_turns_elapsed", w.total_turns_elapsed},
    };
}

void from_json(const nlohmann::json &j, World &w) {
    j.at("clock").get_to(w.clock);
    w.locations = j.value("locations", std::unordered_map<std::string, Location>{});
    w.npcs = j.value("npcs", std::unordered_map<std::string, Npc>{});
    w.items = j.value("items", std::unordered_map<std::string, Item>{});
    j.at("player").get_to(w.player);
    w.flags = j.value("flags", std::unordered_map<std::string, bool>{});
    w.events = j.value("events", std::vector<EventTrigger>{});
    w.total_turns_elapsed = j.value("total_turns_elapsed", 0);
}

} // namespace chronicle
