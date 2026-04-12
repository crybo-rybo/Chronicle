#pragma once
#include "entities/memory_entry.hpp"
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

namespace chronicle {

// Canonical mood vocabulary — ToolRegistry validates against this set.
inline const std::set<std::string> kValidMoods = {"neutral",  "suspicious", "friendly",
                                                   "hostile",  "fearful",    "grieving"};

struct NpcIdentity {
    std::string id;
    std::string name;
    std::string role;
    std::string personality_summary;
    std::string backstory;
    std::string secret;
    std::vector<std::string> goals;
    std::vector<std::string> knowledge;
    int trust_reveal_threshold = 70;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NpcIdentity, id, name, role, personality_summary,
                                                backstory, secret, goals, knowledge,
                                                trust_reveal_threshold)

struct NpcState {
    std::string current_location;
    std::string mood; // see kValidMoods for valid values
    int trust_toward_player = 0; // -100 to 100
    std::vector<std::string> inventory;
    std::vector<MemoryEntry> memories;
    bool has_met_player = false;
    bool secret_revealed = false;

    bool has_item(const std::string& item_id) const;
    void add_memory(MemoryEntry entry);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NpcState, current_location, mood,
                                                trust_toward_player, inventory, memories,
                                                has_met_player, secret_revealed)

struct Npc {
    NpcIdentity identity;
    NpcState state;

    /// Returns trust_toward_player from state.
    int trust_level() const;
    /// Returns true if state.mood == "hostile".
    bool is_hostile() const;
};

void to_json(nlohmann::json& j, const Npc& npc);
void from_json(const nlohmann::json& j, Npc& npc);

} // namespace chronicle
