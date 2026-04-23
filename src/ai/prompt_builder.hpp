/**
 * @file prompt_builder.hpp
 * @brief Assembles LLM system prompts and user turns for NPC conversations.
 *
 * @details @ref PromptBuilder is the single point responsible for transforming
 * structured game data into the text that reaches the language model.  It
 * enforces a token budget for the NPC memory section so that memories remain
 * within a bounded context allocation.
 *
 * @note Only @c max_memory_tokens is currently enforced.  The world-context
 * and conversation-history budgets are reserved but not yet trimmed —
 * implementing @c select_world_context and @c select_history helpers is a
 * Sprint 4 TODO.
 *
 * ### Prompt structure
 * Each NPC conversation starts with a static system prompt built from the
 * following sections, in order:
 * 1. **Identity** — name, role, personality summary.
 * 2. **Background** — backstory (if non-empty).
 * 3. **Goals** — the NPC's active objectives.
 * 4. **Knowledge** — facts the NPC knows.
 * 5. **Rules** — in-character behaviour constraints.
 *
 * Per-turn dynamic context is built separately and injected by the engine as a
 * mid-conversation system message before the player's next dialogue turn.
 *
 * ### Memory selection algorithm
 * Memories are selected in two phases:
 * - Phase 1: High-importance memories (importance ≥ 7) are included first,
 *   sorted by importance descending.
 * - Phase 2: Remaining budget is filled with the most-recent lower-importance
 *   memories, iterating the list in reverse insertion order.
 */

#pragma once
#include "entities/npc.hpp"
#include "entities/world.hpp"
#include <string>
#include <vector>

namespace chronicle {

/// @brief Builds structured LLM prompts from game state.
class PromptBuilder {
  public:
    /// @brief Token-budget configuration for the various prompt sections.
    struct Budget {
        /// @brief Maximum tokens allocated to the NPC memory section.  Default: 800.
        int max_memory_tokens = 800;

        /// @brief Maximum tokens for the world-context section (reserved for future use).
        ///
        /// Not yet enforced; will cap location/NPC/item listings when implemented.
        int max_world_tokens = 400;

        /// @brief Maximum tokens for conversation history (reserved for future use).
        ///
        /// Not yet enforced; will limit the number of included history turns.
        int max_history_tokens = 600;

        /// @brief Heuristic characters-per-token conversion factor.  Default: 4.0.
        ///
        /// Used by @ref estimate_tokens to convert character counts to approximate
        /// token counts when exact tokenisation is unavailable.
        double chars_per_token = 4.0;
    };

    /// @brief Construct with explicit budget settings.
    /// @param budget Token-budget parameters for all prompt sections.
    explicit PromptBuilder(Budget budget);

    /// @brief Assemble the NPC system prompt.
    ///
    /// @details Builds the full system prompt string that establishes the NPC
    /// persona, world context, and behavioural rules for the language model.
    /// The memory section is automatically trimmed to fit within
    /// @c budget.max_memory_tokens.
    ///
    /// @param identity  Immutable NPC identity data.
    /// @param state     Current NPC state (mood, trust, memories, location).
    /// @param world     Read-only world context (clock, locations, other NPCs).
    /// @return The formatted system prompt string.
    std::string build_system_prompt(const NpcIdentity &identity, const NpcState &state,
                                    const World &world) const;

    /// @brief Assemble the static portion of the NPC system prompt.
    ///
    /// @details Produces the system prompt sections that do not change during
    /// a conversation: identity, background, goals, knowledge, and rules.
    /// Called once when a conversation starts.
    ///
    /// @param identity  Immutable NPC identity data.
    /// @param world     Read-only world context (used to resolve knowledge fact IDs).
    /// @return The formatted static system prompt string.
    std::string build_static_system_prompt(const NpcIdentity &identity, const World &world) const;

    /// @brief Assemble the dynamic context block for a single dialogue turn.
    ///
    /// @details Produces the state that may change between turns: mood, trust,
    /// secret (if trust threshold met), memories, and world context (time,
    /// location, NPCs present, visible items).  The engine injects this block as
    /// a mid-conversation system message so the user/assistant history remains
    /// natural dialogue.
    ///
    /// @param identity  NPC identity (used for secret text and threshold).
    /// @param state     Current NPC state (mood, trust, memories, location).
    /// @param world     Read-only world context (clock, locations, other NPCs).
    /// @return The formatted dynamic context string.
    std::string build_dynamic_context(const NpcIdentity &identity, const NpcState &state,
                                      const World &world) const;

    /// @brief Assemble the user-turn message for a single dialogue exchange.
    ///
    /// @details Formats the player's raw input after JSON-encoding it to prevent
    /// prompt injection, then appends a summary of the player's current
    /// inventory.  The optional dynamic context parameter is retained for helper
    /// callers; the game dialogue path injects dynamic context separately as a
    /// system message.
    ///
    /// @param player_input    The raw dialogue text typed by the player.
    /// @param player          The player's current state, used for inventory context.
    /// @param dynamic_context Optional dynamic state context to prepend.
    /// @return The formatted user message string.
    std::string build_user_turn(const std::string &player_input, const Player &player,
                                const std::string &dynamic_context = "") const;

    /// @brief Estimate the token count of a text string.
    ///
    /// @details Uses the @c chars_per_token heuristic from @ref Budget.  This
    /// is intentionally approximate; accurate tokenisation would require loading
    /// the model's vocabulary.
    ///
    /// @param text The string to estimate.
    /// @return Estimated token count, rounded up.
    int estimate_tokens(const std::string &text) const;

  private:
    Budget budget_; ///< Token-budget configuration.

    /// @brief Select memories that fit within @p token_budget.
    ///
    /// @details Implements the two-phase selection algorithm: high-importance
    /// memories first (importance ≥ 7, sorted by importance descending), then
    /// the most-recent lower-importance memories filling the remaining budget.
    ///
    /// @param all          The complete list of memories in chronological order.
    /// @param token_budget Maximum token budget for all selected memories combined.
    /// @return A subset of @p all that fits within the budget.
    std::vector<MemoryEntry> select_memories(const std::vector<MemoryEntry> &all,
                                             int token_budget) const;
};

} // namespace chronicle
