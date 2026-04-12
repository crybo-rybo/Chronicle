#pragma once
#include "entities/npc.hpp"
#include "entities/world.hpp"
#include <string>
#include <vector>

namespace chronicle {

class PromptBuilder {
public:
    struct Budget {
        /// Active: limits NPC memory section in system prompt.
        int max_memory_tokens = 800;
        /// Sprint 3+: will limit world context section when ResponseHandler is implemented.
        int max_world_tokens = 400;
        /// Sprint 3+: will limit conversation history turns when history management is added.
        int max_history_tokens = 600;
        /// Heuristic conversion factor for token estimation (chars per token).
        double chars_per_token = 4.0;
    };

    explicit PromptBuilder(Budget budget);

    std::string build_system_prompt(const NpcIdentity &identity, const NpcState &state,
                                    const World &world) const;

    std::string build_user_turn(const std::string &player_input, const Player &player) const;

    int estimate_tokens(const std::string &text) const;

private:
    Budget budget_;
    std::vector<MemoryEntry> select_memories(const std::vector<MemoryEntry> &all,
                                             int token_budget) const;
};

} // namespace chronicle
