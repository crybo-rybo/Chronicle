#include "entities/npc.hpp"

namespace chronicle {

void to_json(nlohmann::json& j, const Npc& npc) {
    j = nlohmann::json{{"identity", npc.identity}, {"state", npc.state}};
}

void from_json(const nlohmann::json& j, Npc& npc) {
    j.at("identity").get_to(npc.identity);
    j.at("state").get_to(npc.state);
}

} // namespace chronicle
