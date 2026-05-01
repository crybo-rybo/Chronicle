/**
 * @file npc.hpp
 * @brief NPC (non-player character) entity definitions for Chronicle.
 *
 * @details Each NPC is represented by two separate structs that partition data
 * by mutability:
 *
 * - @ref NpcIdentity holds the fixed "who am I" information loaded from the
 *   scenario data files (name, role, personality, backstory, secrets, goals).
 *   It is read-only during gameplay.
 *
 * - @ref NpcState holds the dynamic "what is happening to me right now"
 *   information that changes as the player interacts with the world (location,
 *   mood, trust, inventory, memories).
 *
 * The @ref Npc aggregate bundles both structs together as the canonical entity
 * stored in @ref World::npcs.
 */

#pragma once
#include "entities/memory_entry.hpp"
#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace chronicle {

/// @brief Canonical mood vocabulary — validated by @ref ToolRegistry against this set.
///
/// Any LLM tool call that attempts to set an NPC's mood to a value not in this
/// set will be rejected with an error returned to the model.
/// Uses a sorted constexpr array of string_view to avoid heap allocation at static init
/// and enable binary search for O(log n) lookup without hashing overhead.
inline constexpr std::array<std::string_view, 6> kValidMoods = {{
    "fearful",
    "friendly",
    "grieving",
    "hostile",
    "neutral",
    "suspicious",
}};

/// @brief Test whether a mood string is in the valid mood vocabulary.
/// @param mood The mood string to check.
/// @return @c true if @p mood is a recognised mood value.
inline bool is_valid_mood(std::string_view mood) {
    // Array is sorted, so binary search is correct.
    auto it = std::lower_bound(kValidMoods.begin(), kValidMoods.end(), mood);
    return it != kValidMoods.end() && *it == mood;
}

/// @brief Stable v1 NPC tool palette exposed to scenario authors.
inline constexpr std::array<std::string_view, 10> kValidNpcTools = {{
    "give_item",
    "inspect_item",
    "move_self",
    "remember",
    "reveal_knowledge",
    "say",
    "set_flag",
    "take_item",
    "update_mood",
    "update_trust",
}};

/// @brief Return the default allowlist for an NPC with no explicit tool policy.
inline std::vector<std::string> default_allowed_npc_tools() {
    std::vector<std::string> tools;
    tools.reserve(kValidNpcTools.size());
    for (auto tool : kValidNpcTools) {
        tools.emplace_back(tool);
    }
    return tools;
}

/// @brief Test whether a tool string belongs to the built-in v1 NPC tool palette.
inline bool is_valid_npc_tool(std::string_view tool) {
    auto it = std::lower_bound(kValidNpcTools.begin(), kValidNpcTools.end(), tool);
    return it != kValidNpcTools.end() && *it == tool;
}

/// @brief Per-NPC allowlist controlling which framework tools and IDs the LLM may use.
///
/// @details Empty scoped ID lists mean "no additional restriction."  An empty
/// @c allowed_tools list means the NPC has no tool permissions.
struct NpcToolPolicy {
    std::vector<std::string> allowed_tools = default_allowed_npc_tools();
    std::vector<std::string> allowed_items;
    std::vector<std::string> allowed_facts;
    std::vector<std::string> allowed_flags;
    std::vector<std::string> allowed_locations;
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NpcToolPolicy, allowed_tools, allowed_items,
                                                allowed_facts, allowed_flags, allowed_locations)
/// @endcond

/// @brief Immutable identity data for a single NPC.
///
/// @details Loaded from @c npcs.json at startup and never modified during
/// gameplay.  All fields are used by @ref PromptBuilder when constructing the
/// system prompt that gives the LLM its NPC persona.
struct NpcIdentity {
    /// Unique string identifier, matches the JSON map key.
    std::string id;

    /// Display name shown to the player, e.g. @c "Elena".
    std::string name;

    /// Short occupational label used in the system prompt, e.g. @c "innkeeper".
    std::string role;

    /// @brief Free-text personality description injected into the LLM system prompt.
    ///
    /// Should describe temperament, speaking style, and defining character traits.
    std::string personality_summary;

    /// @brief Private backstory known only to this NPC (and always in the system prompt).
    ///
    /// The player cannot read this directly; the LLM uses it to motivate
    /// in-character responses.
    std::string backstory;

    /// @brief Secret the NPC is hiding.
    ///
    /// Only injected into the system prompt when the player's trust level
    /// meets or exceeds @c trust_reveal_threshold.
    std::string secret;

    /// List of narrative objectives this NPC is pursuing.  Injected into the system prompt.
    std::vector<std::string> goals;

    /// @brief Set of fact IDs this NPC can disclose via the @c reveal_knowledge tool.
    ///
    /// Each string is a fact ID defined in the world data.  The @ref ToolRegistry
    /// validates that only facts present in this list can be revealed.
    std::vector<std::string> knowledge;

    /// @brief Minimum trust score required before the NPC's secret is shown in the prompt.
    ///
    /// Defaults to 70 (out of 100).
    int trust_reveal_threshold = 70;

    /// @brief Scenario-authored tool permissions for this NPC.
    NpcToolPolicy tool_policy;
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NpcIdentity, id, name, role, personality_summary,
                                                backstory, secret, goals, knowledge,
                                                trust_reveal_threshold, tool_policy)
/// @endcond

/// @brief Mutable runtime state for a single NPC.
///
/// @details All fields in this struct can change during gameplay through
/// validated @ref MutationRequest operations processed by @ref GameEngine.
/// The entire struct is serialised to JSON during save/load.
struct NpcState {
    /// ID of the @ref Location where this NPC currently resides.
    std::string current_location;

    /// @brief Current emotional state.
    ///
    /// Must be one of the values in @ref kValidMoods.  Mutations that attempt
    /// to set an invalid mood are rejected by @ref ToolRegistry.
    std::string mood;

    /// @brief How much this NPC trusts the player, in the range [-100, 100].
    ///
    /// Positive values indicate goodwill; negative values indicate hostility.
    /// Changes are clamped to this range by the mutation pipeline.
    int trust_toward_player = 0;

    /// IDs of @ref Item objects held by this NPC.
    std::vector<std::string> inventory;

    /// Chronological list of memory summaries.  See @ref MemoryEntry.
    std::vector<MemoryEntry> memories;

    /// @c true after the player has spoken to this NPC at least once.
    bool has_met_player = false;

    /// @c true once the NPC has revealed their secret to the player.
    bool secret_revealed = false;

    /// @brief Test whether this NPC currently holds a specific item.
    /// @param item_id The item ID to search for.
    /// @return @c true if @p item_id appears in @c inventory.
    bool has_item(const std::string &item_id) const;

    /// @brief Append a new memory to the end of the memory list.
    /// @param entry The memory entry to add.
    void add_memory(MemoryEntry entry);
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NpcState, current_location, mood,
                                                trust_toward_player, inventory, memories,
                                                has_met_player, secret_revealed)
/// @endcond

/// @brief Aggregate NPC entity combining immutable identity with mutable state.
///
/// @details This is the type stored in @ref World::npcs.  Identity data is read
/// frequently by the AI layer; state data is read and written by the mutation
/// pipeline.
struct Npc {
    NpcIdentity identity; ///< Immutable persona and backstory information.
    NpcState state;       ///< Mutable runtime state.

    /// @brief Convenience accessor for the NPC's current trust score.
    /// @return @c state.trust_toward_player.
    int trust_level() const;

    /// @brief Test whether this NPC is currently hostile toward the player.
    /// @return @c true if @c state.mood == @c "hostile".
    bool is_hostile() const;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const Npc &npc);
void from_json(const nlohmann::json &j, Npc &npc);
/// @endcond

} // namespace chronicle
