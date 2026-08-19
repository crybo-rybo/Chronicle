#include "chronicle/prompt.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>

namespace chronicle {

namespace {

std::size_t utf8_prefix(const std::string &text, std::size_t bytes) {
    bytes = std::min(bytes, text.size());
    while (bytes > 0 && bytes < text.size() &&
           (static_cast<unsigned char>(text[bytes]) & 0xC0U) == 0x80U) {
        --bytes;
    }
    return bytes;
}

std::string bounded_prompt_text(const std::string &text, const int token_budget) {
    const auto byte_budget = static_cast<std::size_t>(std::max(1, token_budget)) * 4U;
    if (text.size() <= byte_budget) {
        return text;
    }
    constexpr std::string_view marker = "\n[truncated]";
    if (byte_budget <= marker.size()) {
        return text.substr(0, utf8_prefix(text, byte_budget));
    }
    const auto prefix = utf8_prefix(text, byte_budget - marker.size());
    return text.substr(0, prefix) + std::string(marker);
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
        << "- Check each tool result's ok field; when false, use error to choose another action.\n"
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

    std::ostringstream context;
    context << "Time: " << world.clock.period_name() << " (turn " << world.clock.turns_elapsed
            << ")\n";
    context << "Your location: " << loc.name << "\n";
    context << "Mood: " << state.mood << "\n";
    context << "Trust toward player: " << state.trust_toward_player << "\n";

    std::ostringstream memory_text;
    auto memories = state.memories;
    std::ranges::stable_sort(memories, [](const MemoryEntry &a, const MemoryEntry &b) {
        return a.importance > b.importance;
    });
    for (const auto &memory : memories) {
        memory_text << "- (" << memory.importance << ") " << memory.summary << "\n";
    }
    if (memories.empty()) {
        memory_text << "- (none)\n";
    }

    std::vector<std::string> visible_npcs;
    for (const auto &[nid, other] : world.npcs) {
        if (nid != npc_id && other.state.current_location == state.current_location) {
            visible_npcs.push_back(other.identity.name);
        }
    }
    context << "Also here: ";
    if (visible_npcs.empty()) {
        context << "no one";
    } else {
        for (std::size_t i = 0; i < visible_npcs.size(); ++i) {
            context << (i == 0 ? "" : ", ") << visible_npcs[i];
        }
    }
    context << "\n";

    std::vector<std::string> visible_items;
    for (const auto &[iid, position] : world.item_positions) {
        const auto item_it = world.items.find(iid);
        if (position.is_location(state.current_location) && item_it != world.items.end() &&
            !item_it->second.hidden) {
            visible_items.push_back(item_it->second.name);
        }
    }
    context << "Visible items: ";
    if (visible_items.empty()) {
        context << "none";
    } else {
        for (std::size_t i = 0; i < visible_items.size(); ++i) {
            context << (i == 0 ? "" : ", ") << visible_items[i];
        }
    }
    context << "\n";

    std::vector<std::string> inventory_names;
    for (const auto &iid : items_at(world, ItemHolder::player)) {
        const auto item = world.items.find(iid);
        if (item != world.items.end()) {
            inventory_names.push_back(item->second.name);
        }
    }
    context << "Player carries: ";
    if (inventory_names.empty()) {
        context << "nothing";
    } else {
        for (std::size_t i = 0; i < inventory_names.size(); ++i) {
            context << (i == 0 ? "" : ", ") << inventory_names[i];
        }
    }
    context << "\n";

    if (!identity.secret.empty() && state.trust_toward_player >= identity.trust_reveal_threshold) {
        context << "Secret you may reveal carefully: " << identity.secret << "\n";
    }
    context << "Location description: " << loc.base_description << "\n";

    std::ostringstream out;
    out << "[Context]\n"
        << bounded_prompt_text(context.str(), world.config.max_world_tokens) << "\nMemories:\n"
        << bounded_prompt_text(memory_text.str(), world.config.max_memory_tokens) << "\n[Player]\n";
    const nlohmann::json payload{
        {"player_said", bounded_prompt_text(player_text, world.config.max_history_tokens)}};
    out << payload.dump();
    return out.str();
}

} // namespace chronicle
