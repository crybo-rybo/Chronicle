#pragma once
#include "entities/npc.hpp"
#include "entities/world.hpp"
#include <string>
#include <vector>

namespace chronicle {

class PromptBuilder {
public:
    struct Budget {
        int max_memory_tokens = 800;
        int max_world_tokens = 400;
        int max_history_tokens = 600;
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
    static constexpr double kCharsPerToken = 4.0;
};

} // namespace chronicle
