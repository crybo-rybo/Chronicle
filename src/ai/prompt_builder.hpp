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
 * ### System prompt structure
 * Each NPC system prompt is built from the following sections, in order:
 * 1. **Identity** — name, role, personality summary.
 * 2. **Background** — backstory (if non-empty).
 * 3. **Secret** — revealed only when trust meets the threshold.
 * 4. **Goals** — the NPC's active objectives.
 * 5. **Knowledge** — facts the NPC knows.
 * 6. **Current state** — mood and trust level.
 * 7. **Memories** — selected within @ref Budget::max_memory_tokens.
 * 8. **World context** — time, location, other characters present, visible items.
 * 9. **Rules** — in-character behaviour constraints.
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

    /// @brief Assemble the user-turn message for a single dialogue exchange.
    ///
    /// @details Wraps the player's raw input as a JSON-encoded quoted string
    /// to prevent prompt-injection, then appends a summary of the player's
    /// current inventory for context.
    ///
    /// @param player_input The raw dialogue text typed by the player.
    /// @param player       The player's current state, used for inventory context.
    /// @return The formatted user message string.
    std::string build_user_turn(const std::string &player_input, const Player &player) const;

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
