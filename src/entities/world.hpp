#pragma once
#include "entities/clock.hpp"
#include "entities/event.hpp"
#include "entities/item.hpp"
#include "entities/location.hpp"
#include "entities/npc.hpp"
#include "entities/player.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace chronicle {

struct World {
    Clock clock;
    std::unordered_map<std::string, Location> locations;
    std::unordered_map<std::string, Npc> npcs;
    std::unordered_map<std::string, Item> items;
    Player player;
    std::unordered_map<std::string, bool> flags;
    std::vector<EventTrigger> events; // vector, not map (id is inside the struct)
    int total_turns_elapsed = 0;
};

void to_json(nlohmann::json &j, const World &w);
void from_json(const nlohmann::json &j, World &w);

} // namespace chronicle
