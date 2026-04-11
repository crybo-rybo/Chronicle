#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

/// A single memory held by an NPC. Generated via structured extraction after conversations.
struct MemoryEntry {
    std::string timestamp;    ///< In-game time string, e.g. "Morning, Day 1"
    std::string type;         ///< "conversation", "observation", "rumor", "event" — validated by the AI response handler layer
    std::string summary;      ///< One- to two-sentence summary
    int importance = 1;       ///< 1 (background noise) to 10 (critical plot event) — range validated by the AI response handler layer
    std::string related_npc;  ///< Optional NPC id involved
    std::string related_item; ///< Optional item id involved
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MemoryEntry, timestamp, type, summary, importance,
                                                related_npc, related_item)

} // namespace chronicle
