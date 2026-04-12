#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace chronicle {

struct Player {
    std::string current_location;
    std::vector<std::string> inventory;
    std::vector<std::string> known_facts;
    std::unordered_map<std::string, int> npc_encounter_count;
    int turns_in_conversation = 0;

    bool has_item(const std::string& item_id) const;
    bool knows_fact(const std::string& fact_id) const;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Player, current_location, inventory, known_facts,
                                                npc_encounter_count, turns_in_conversation)

} // namespace chronicle
