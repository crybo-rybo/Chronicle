/**
 * @file prompt_builder.cpp
 * @brief Implementation of @ref PromptBuilder — system prompt and user turn assembly.
 *
 * @details Implements the structured prompt template, the two-phase memory
 * selection algorithm, and the token estimation heuristic.
 */

#include "ai/prompt_builder.hpp"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>
#include <sstream>

namespace chronicle {

PromptBuilder::PromptBuilder(Budget budget) : budget_(budget) {}

std::string PromptBuilder::build_system_prompt(const NpcIdentity &identity, const NpcState &state,
                                               const World &world) const {
    std::ostringstream out;

    // 1. Identity
    out << "You are " << identity.name << ", " << identity.role << ".\n"
        << identity.personality_summary << "\n";

    // 2. Backstory (only if non-empty)
    if (!identity.backstory.empty()) {
        out << "\nBackground:\n" << identity.backstory << "\n";
    }

    // 3. Secret (only if trust is high enough)
    if (state.trust_toward_player >= identity.trust_reveal_threshold) {
        out << "\nSecret: " << identity.secret << "\n";
    }

    // 4. Goals (skip if empty)
    if (!identity.goals.empty()) {
        out << "\nYour goals:\n";
        for (const auto &goal : identity.goals) {
            out << "- " << goal << "\n";
        }
    }

    // 5. Knowledge (skip if empty; resolve fact IDs to authored text)
    if (!identity.knowledge.empty()) {
        out << "\nWhat you know:\n";
        for (const auto &fact_id : identity.knowledge) {
            if (auto it = world.facts.find(fact_id); it != world.facts.end()) {
                out << "- " << it->second.text << "\n";
            } else {
                out << "- " << fact_id << "\n";
            }
        }
    }

    // 6. Current state
    out << "\nCurrent mood: " << state.mood << "\n";
    out << "Trust toward the player: " << state.trust_toward_player << "/100\n";

    // 7. Memories (selected within budget)
    auto selected = select_memories(state.memories, budget_.max_memory_tokens);
    if (!selected.empty()) {
        out << "\nWhat you remember:\n";
        for (const auto &mem : selected) {
            out << "- [" << mem.type << "] " << mem.summary << "\n";
        }
    }

    // 8. World context
    out << "\nCurrent time: " << world.clock.to_prompt_string() << "\n";

    // Look up location name
    auto loc_it = world.locations.find(state.current_location);
    if (loc_it != world.locations.end()) {
        out << "Location: " << loc_it->second.name << "\n";
    } else {
        out << "Location: " << state.current_location << "\n";
    }

    // Player presence
    if (world.player.current_location == state.current_location) {
        out << "The player is here.\n";
    }

    // Other NPCs present
    if (loc_it != world.locations.end()) {
        for (const auto &npc_id : loc_it->second.npcs) {
            if (npc_id == identity.id) {
                continue;
            }
            auto npc_it = world.npcs.find(npc_id);
            if (npc_it != world.npcs.end()) {
                out << npc_it->second.identity.name << " is also here.\n";
            }
        }

        // Visible items
        for (const auto &item_id : loc_it->second.items) {
            auto item_it = world.items.find(item_id);
            if (item_it != world.items.end() && !item_it->second.hidden) {
                out << "You can see: " << item_it->second.name << "\n";
            }
        }
    }

    // 9. Rules block
    out << "\nRules:\n";
    out << "- Stay in character as " << identity.name << "\n";
    out << "- Speak to the player using normal dialogue text\n";
    out << "- Use provided tools for actions, do not describe them in dialogue\n";
    out << "- Do not invent facts about the world\n";
    out << "- Your secret can only be revealed if the player has earned sufficient trust\n";
    out << "- Only call tools when world state actually changes as a direct result of this "
           "exchange (e.g. giving an item, moving to another location); "
           "do not call tools for greetings or casual conversation where nothing changes\n";

    return out.str();
}

std::string PromptBuilder::build_static_system_prompt(const NpcIdentity &identity,
                                                      const World &world) const {
    std::ostringstream out;

    // 1. Identity
    out << "You are " << identity.name << ", " << identity.role << ".\n"
        << identity.personality_summary << "\n";

    // 2. Backstory (only if non-empty)
    if (!identity.backstory.empty()) {
        out << "\nBackground:\n" << identity.backstory << "\n";
    }

    // 3. Goals (skip if empty)
    if (!identity.goals.empty()) {
        out << "\nYour goals:\n";
        for (const auto &goal : identity.goals) {
            out << "- " << goal << "\n";
        }
    }

    // 4. Knowledge (skip if empty; resolve fact IDs to authored text)
    if (!identity.knowledge.empty()) {
        out << "\nWhat you know:\n";
        for (const auto &fact_id : identity.knowledge) {
            if (auto it = world.facts.find(fact_id); it != world.facts.end()) {
                out << "- " << it->second.text << "\n";
            } else {
                out << "- " << fact_id << "\n";
            }
        }
    }

    // 5. Rules block
    out << "\nRules:\n";
    out << "- Stay in character as " << identity.name << "\n";
    out << "- Speak to the player using normal dialogue text\n";
    out << "- Use provided tools for actions, do not describe them in dialogue\n";
    out << "- Do not invent facts about the world\n";
    out << "- Your secret can only be revealed if the player has earned sufficient trust\n";
    out << "- Only call tools when world state actually changes as a direct result of this "
           "exchange (e.g. giving an item, moving to another location); "
           "do not call tools for greetings or casual conversation where nothing changes\n";

    return out.str();
}

std::string PromptBuilder::build_user_turn(const std::string &player_input,
                                           const Player &player) const {
    std::ostringstream out;
    nlohmann::json encoded = player_input;
    out << "The player says: " << encoded.dump();

    if (!player.inventory.empty()) {
        out << "\n\nPlayer inventory: ";
        for (size_t i = 0; i < player.inventory.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << player.inventory[i];
        }
    }

    return out.str();
}

int PromptBuilder::estimate_tokens(const std::string &text) const {
    return static_cast<int>(std::ceil(text.size() / budget_.chars_per_token));
}

std::vector<MemoryEntry> PromptBuilder::select_memories(const std::vector<MemoryEntry> &all,
                                                        int token_budget) const {
    if (all.empty()) {
        return {};
    }

    static constexpr int kHighImportanceThreshold = 7;

    // Phase 1: Collect indices of high-importance memories, sorted by importance desc.
    // Using indices avoids copying MemoryEntry objects (which contain std::strings)
    // into a temporary vector just for sorting.
    std::vector<std::size_t> high_indices;
    high_indices.reserve(all.size()); // upper bound
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i].importance >= kHighImportanceThreshold) {
            high_indices.push_back(i);
        }
    }
    // stable_sort on indices preserves chronological order as tiebreaker.
    std::ranges::stable_sort(high_indices, [&all](std::size_t a, std::size_t b) {
        return all[a].importance > all[b].importance;
    });

    std::vector<MemoryEntry> selected;
    int tokens_used = 0;

    for (std::size_t idx : high_indices) {
        int cost = estimate_tokens(all[idx].summary);
        if (tokens_used + cost > token_budget) {
            continue; // skip oversized, try next
        }
        tokens_used += cost;
        selected.push_back(all[idx]);
    }

    // Phase 2: Fill remaining budget with most-recent-first from lower-importance memories.
    // Reverse iteration of `all` (insertion order) gives recency ordering.
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
        if (it->importance >= kHighImportanceThreshold) {
            continue; // already considered in phase 1
        }
        int cost = estimate_tokens(it->summary);
        if (tokens_used + cost > token_budget) {
            continue; // skip oversized
        }
        tokens_used += cost;
        selected.push_back(*it);
    }

    return selected;
}

} // namespace chronicle
