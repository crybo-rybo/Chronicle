#include "entities/config.hpp"
#include <fstream>
#include <stdexcept>

namespace chronicle {

std::unordered_map<std::string, std::string> default_mutation_narration_templates() {
    return {
        {"give_item_to_player", "{npc} hands you the {item}."},
        {"take_item_from_player", "{npc} takes the {item}."},
        {"update_npc_mood", "{npc}'s expression shifts - they seem {mood} now."},
        {"move_npc", "{npc} excuses themselves and leaves."},
        {"reveal_knowledge", ""},
        {"update_npc_trust", ""},
        {"add_memory", ""},
        {"set_flag", ""},
    };
}

Config Config::load(const std::filesystem::path &path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Config: cannot open file: " + path.string());
    try {
        return nlohmann::json::parse(f).get<Config>();
    } catch (const nlohmann::json::exception &e) {
        throw std::runtime_error("Config: parse error in " + path.string() + ": " + e.what());
    }
}

void Config::save(const std::filesystem::path &path) const {
    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Config: cannot write file: " + path.string());
    nlohmann::json j = *this;
    f << j.dump(2) << '\n';
    if (!f.good())
        throw std::runtime_error("Config: write failed for " + path.string());
}

} // namespace chronicle
