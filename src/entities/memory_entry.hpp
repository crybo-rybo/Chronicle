/**
 * @file memory_entry.hpp
 * @brief NPC memory record used for long-term contextual recall.
 *
 * @details Chronicle 1.0 memories are explicit: authors may seed them in
 * @c npcs.json, and NPCs may add them through the validated @c remember tool.
 * These summaries are persisted as part of @ref NpcState and injected back
 * into future system prompts by the @ref PromptBuilder, giving each NPC a
 * sense of history without storing full conversation transcripts.
 */

#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

/// @brief A single condensed memory held by an NPC.
///
/// @details Memories are authored in scenario data or added by the
/// @c remember tool after significant interactions.  They serve as the NPC's
/// long-term recall mechanism: the @ref PromptBuilder selects memories within a
/// token budget and injects them into the system prompt so the LLM can reason
/// about past events without receiving full conversation history.
///
/// @note Tool-created memories clamp @c importance before committing the entry
/// to @ref NpcState; JSON Schema documents the same authored range.
struct MemoryEntry {
    /// In-game timestamp at which the memory was created, e.g. @c "Morning, Day 1".
    std::string timestamp;

    /// Categorises the nature of the memory.  Valid values are
    /// @c "conversation", @c "observation", @c "rumor", and @c "event".
    std::string type;

    /// One- to two-sentence human-readable summary of what happened.
    std::string summary;

    /// Salience score from 1 (background noise) to 10 (critical plot event).
    /// Used by @ref PromptBuilder to prioritise which memories fit within the
    /// token budget.  Clamped to [1, 10] by the validation layer.
    int importance = 1;

    /// Optional ID of an NPC directly involved in this memory.  Empty if none.
    std::string related_npc;

    /// Optional ID of an item directly involved in this memory.  Empty if none.
    std::string related_item;
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MemoryEntry, timestamp, type, summary, importance,
                                                related_npc, related_item)
/// @endcond

} // namespace chronicle
