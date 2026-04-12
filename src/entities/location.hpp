#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace chronicle {

struct World; // forward declaration — defined in world.hpp

struct Location {
    std::string id;
    std::string name;
    std::string base_description;
    std::string current_description;
    std::map<std::string, std::string> exits; // direction -> location_id
    std::vector<std::string> items;           // item ids present here
    std::vector<std::string> npcs;            // npc ids present here
    std::vector<std::string> locked_exits;    // direction strings that are locked

    /// Returns current_description if non-empty, else base_description.
    /// Sprint 1: World parameter unused; future sprints will add NPC/item listing.
    std::string describe(const World &world) const;
};

void to_json(nlohmann::json &j, const Location &loc);
void from_json(const nlohmann::json &j, Location &loc);

} // namespace chronicle
