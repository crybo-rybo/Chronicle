// Prompt assembly for NPC turns.
//
// The static system prompt (identity, knowledge, rules) is fixed for the life
// of an NPC conversation; per-turn world state (time, mood, trust, memories)
// travels in the user message so persistent conversation history stays valid.
#pragma once

#include <string>

#include "chronicle/cartridge/models.hpp"

namespace chronicle {

[[nodiscard]] std::string build_npc_system_prompt(const WorldState &world,
                                                  const std::string &npc_id);

[[nodiscard]] std::string build_npc_turn_message(const WorldState &world, const std::string &npc_id,
                                                 const std::string &player_text);

} // namespace chronicle
