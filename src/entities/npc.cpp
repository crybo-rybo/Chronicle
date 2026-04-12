#include "entities/npc.hpp"

#include <algorithm>

namespace chronicle {

bool NpcState::has_item(const std::string& item_id) const {
    return std::ranges::find(inventory, item_id) != inventory.end();
}

void NpcState::add_memory(MemoryEntry entry) {
    memories.push_back(std::move(entry));
}

int Npc::trust_level() const {
    return state.trust_toward_player;
}

bool Npc::is_hostile() const {
    return state.mood == "hostile";
}

void to_json(nlohmann::json& j, const Npc& npc) {
    j = nlohmann::json{{"identity", npc.identity}, {"state", npc.state}};
}

void from_json(const nlohmann::json& j, Npc& npc) {
    j.at("identity").get_to(npc.identity);
    j.at("state").get_to(npc.state);
}

} // namespace chronicle
