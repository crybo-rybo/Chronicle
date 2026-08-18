#include "chronicle/prompt.hpp"

#include <algorithm>
#include <sstream>

namespace chronicle {

namespace {

int word_count(const std::string &text) {
    std::istringstream stream(text);
    std::string word;
    int count = 0;
    while (stream >> word) {
        ++count;
    }
    return count;
}

} // namespace

std::string build_npc_system_prompt(const WorldState &world, const std::string &npc_id) {
    const auto &npc = world.npcs.at(npc_id);
    const auto &identity = npc.identity;

    std::ostringstream out;
    out << "You are " << identity.name << ", " << identity.role << ".\n";
    out << "Personality: " << identity.personality_summary << "\n";
    out << "Backstory: " << identity.backstory << "\n";
    out << "Goals:\n";
    for (const auto &goal : identity.goals) {
        out << "- " << goal << "\n";
    }
    out << "Knowledge:\n";
    bool any_knowledge = false;
    for (const auto &fact_id : identity.knowledge) {
        const auto fact = world.facts.find(fact_id);
        if (fact != world.facts.end()) {
            out << "- [" << fact_id << "] " << fact->second.text << "\n";
            any_knowledge = true;
        }
    }
    if (!any_knowledge) {
        out << "- (none)\n";
    }
    out << "Rules:\n"
        << "- Stay in character.\n"
        << "- Use tools to act; do not invent facts that are not in your knowledge list.\n"
        << "- Only reveal secrets when trust is high enough and the secret is authored.\n"
        << "- Prefer the say tool for spoken dialogue.";
    return out.str();
}

std::string build_npc_turn_message(const WorldState &world, const std::string &npc_id,
                                   const std::string &player_text) {
    const auto &npc = world.npcs.at(npc_id);
    const auto &state = npc.state;
    const auto &identity = npc.identity;
    const auto &loc = world.locations.at(state.current_location);

    std::ostringstream out;
    out << "[Context]\n";
    out << "Time: " << world.clock.period_name() << " (turn " << world.clock.turns_elapsed << ")\n";
    out << "Your location: " << loc.name << " — " << loc.base_description << "\n";
    out << "Mood: " << state.mood << "\n";
    out << "Trust toward player: " << state.trust_toward_player << "\n";
    if (!identity.secret.empty() && state.trust_toward_player >= identity.trust_reveal_threshold) {
        out << "Secret you may reveal carefully: " << identity.secret << "\n";
    }

    out << "Memories:\n";
    auto memories = state.memories;
    std::ranges::stable_sort(memories, [](const MemoryEntry &a, const MemoryEntry &b) {
        return a.importance > b.importance;
    });
    const int budget = world.config.max_memory_tokens / 4; // rough token proxy
    int used = 0;
    bool any_memory = false;
    for (const auto &memory : memories) {
        const std::string line = "- (" + std::to_string(memory.importance) + ") " + memory.summary;
        used += word_count(line);
        if (used > budget) {
            break;
        }
        out << line << "\n";
        any_memory = true;
    }
    if (!any_memory) {
        out << "- (none)\n";
    }

    std::vector<std::string> visible_npcs;
    for (const auto &[nid, other] : world.npcs) {
        if (nid != npc_id && other.state.current_location == state.current_location) {
            visible_npcs.push_back(other.identity.name);
        }
    }
    out << "Also here: ";
    if (visible_npcs.empty()) {
        out << "no one";
    } else {
        for (std::size_t i = 0; i < visible_npcs.size(); ++i) {
            out << (i == 0 ? "" : ", ") << visible_npcs[i];
        }
    }
    out << "\n";

    std::vector<std::string> visible_items;
    for (const auto &[iid, position] : world.item_positions) {
        const auto item_it = world.items.find(iid);
        if (position.is_location(state.current_location) && item_it != world.items.end() &&
            !item_it->second.hidden) {
            visible_items.push_back(item_it->second.name);
        }
    }
    out << "Visible items: ";
    if (visible_items.empty()) {
        out << "none";
    } else {
        for (std::size_t i = 0; i < visible_items.size(); ++i) {
            out << (i == 0 ? "" : ", ") << visible_items[i];
        }
    }
    out << "\n\n[Player]\n";

    nlohmann::json inventory_names = nlohmann::json::array();
    for (const auto &iid : items_at(world, ItemHolder::player)) {
        const auto item = world.items.find(iid);
        if (item != world.items.end()) {
            inventory_names.push_back(item->second.name);
        }
    }
    const nlohmann::json payload{{"player_said", player_text},
                                 {"player_inventory", inventory_names}};
    out << payload.dump();
    return out.str();
}

} // namespace chronicle
