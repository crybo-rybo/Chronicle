#include "ai/prompt_builder.hpp"
#include <algorithm>
#include <cmath>
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

    // 5. Knowledge (skip if empty)
    if (!identity.knowledge.empty()) {
        out << "\nWhat you know:\n";
        for (const auto &fact : identity.knowledge) {
            out << "- " << fact << "\n";
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
    out << "- Use provided tools for actions, do not describe them in dialogue\n";
    out << "- Do not invent facts about the world\n";
    out << "- Your secret can only be revealed if the player has earned sufficient trust\n";

    return out.str();
}

std::string PromptBuilder::build_user_turn(const std::string &player_input,
                                           const Player &player) const {
    std::ostringstream out;
    out << "The player says: \"" << player_input << "\"";

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
    return static_cast<int>(std::ceil(text.size() / kCharsPerToken));
}

std::vector<MemoryEntry> PromptBuilder::select_memories(const std::vector<MemoryEntry> &all,
                                                        int token_budget) const {
    if (all.empty()) {
        return {};
    }

    // Copy and stable-sort by importance descending
    auto sorted = all;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const MemoryEntry &a, const MemoryEntry &b) {
                         return a.importance > b.importance;
                     });

    std::vector<MemoryEntry> selected;
    int tokens_used = 0;

    for (const auto &mem : sorted) {
        int cost = estimate_tokens(mem.summary);
        if (tokens_used + cost > token_budget) {
            break;
        }
        tokens_used += cost;
        selected.push_back(mem);
    }

    return selected;
}

} // namespace chronicle
