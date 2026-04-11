#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

/// A game item. Items have a single canonical entry in World::items; ownership is tracked
/// by item ID in player/npc/location containers. Items never move from the registry —
/// only their ownership location changes.
struct Item {
    /// Unique identifier. Set from the JSON map key during initial world load.
    /// Also serialized in to_json/from_json for save/load roundtrips.
    /// The world loader is the canonical source of truth for this value.
    std::string id;
    std::string name;
    std::string description;
    bool takeable = true;
    bool key_item = false;     ///< Cannot be dropped or traded if true
    bool hidden = false;       ///< Not shown in room description by default
    std::string unlock_target; ///< Location exit this item unlocks, if any (empty = none)
    std::map<std::string, std::string> properties; ///< Extensible key/value metadata
};

void to_json(nlohmann::json &j, const Item &item);
void from_json(const nlohmann::json &j, Item &item);

} // namespace chronicle
