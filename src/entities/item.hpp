#pragma once
#include <map>
#include <string>
#include <nlohmann/json.hpp>

namespace chronicle {

/// A game item. Items have a single canonical entry in World::items; ownership is tracked
/// by item ID in player/npc/location containers. Items never move from the registry —
/// only their ownership location changes.
struct Item {
    std::string id;           ///< Unique identifier (set from JSON map key on load)
    std::string name;
    std::string description;
    bool takeable = true;
    bool key_item = false;    ///< Cannot be dropped or traded if true
    bool hidden = false;      ///< Not shown in room description by default
    std::string unlock_target; ///< Location exit this item unlocks, if any (empty = none)
    std::map<std::string, std::string> properties; ///< Extensible key/value metadata
};

void to_json(nlohmann::json& j, const Item& item);
void from_json(const nlohmann::json& j, Item& item);

} // namespace chronicle
