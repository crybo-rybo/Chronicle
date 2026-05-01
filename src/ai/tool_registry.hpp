/**
 * @file tool_registry.hpp
 * @brief Validation, queuing, and registration of NPC tool calls.
 *
 * @details @ref ToolRegistry sits at the boundary between the LLM and the
 * game world.  It serves three roles:
 *
 * 1. **Validation** — each @c validate_* method checks a proposed state change
 *    against the current @ref World and returns either a @ref MutationRequest
 *    ready to apply, or an error string to send back to the model.
 *
 * 2. **Queuing** — each @c register_* method validates the change and, if
 *    valid, enqueues a @ref MutationRequest in the pending list.  The pending
 *    list is drained by @ref GameEngine::process_pending_mutations after
 *    inference completes.
 *
 * 3. **Zoo-Keeper registration** — @ref register_tools wires all game tools
 *    onto a @c zoo::Agent so the LLM can call them during inference.
 *
 * ### Tool call flow
 * During inference, the Zoo-Keeper agent invokes registered tool lambdas on the
 * inference thread.  Each lambda calls the corresponding @c register_* method,
 * which validates against the current @ref World through a @c const reference
 * and either enqueues a mutation or returns an error string.  Tool results are
 * returned synchronously to Zoo-Keeper so the model can react before generating
 * its next token.  The actual world mutation happens later, on the main thread,
 * after inference completes.
 *
 * This class is not a broad concurrent data structure.  It assumes the engine
 * does not mutate @ref World while an inference turn is actively calling tools.
 *
 * @note @ref ToolRegistry holds a @c const reference to the @ref World.  It
 * reads world state for validation but never writes to it.
 */

#pragma once
#include "engine/mutation_request.hpp"
#include "entities/world.hpp"
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace zoo {
class Agent;
} // namespace zoo

namespace chronicle {

/// @brief Validates proposed NPC tool calls and maintains the pending mutation queue.
class ToolRegistry {
  public:
    /// @brief Return type for validate_* methods.
    ///
    /// Holds either a valid @ref MutationRequest on success, or a @c std::string
    /// error message to return to the LLM on failure.
    using ValidationResult = std::variant<MutationRequest, std::string>;

    /// @brief Construct with a read-only view of the current world state.
    ///
    /// @details The reference is used for validation throughout the lifetime of
    /// this registry.  The caller must ensure the @ref World outlives the registry.
    ///
    /// @param world The world to validate mutations against.
    /// @param sink  Callback invoked for each validated mutation.  If null,
    ///              mutations are pushed to an internal vector accessible via
    ///              @ref pending_mutations (test/standalone convenience).
    explicit ToolRegistry(const World &world, MutationSink sink = nullptr);

    // -----------------------------------------------------------------------
    // Validation-only methods (do not enqueue)
    // -----------------------------------------------------------------------

    /// @brief Validate a give_item request.
    /// @param npc_id  The NPC attempting to give an item.
    /// @param item_id The item to give.
    /// @return A @ref MutationRequest on success, or an error string.
    ValidationResult validate_give_item(const std::string &npc_id,
                                        const std::string &item_id) const;

    /// @brief Validate a take_item request.
    /// @param npc_id  The NPC attempting to take an item.
    /// @param item_id The item to take.
    /// @return A @ref MutationRequest on success, or an error string.
    ValidationResult validate_take_item(const std::string &npc_id,
                                        const std::string &item_id) const;

    /// @brief Validate an update_mood request.
    /// @param npc_id The NPC whose mood would change.
    /// @param mood   The proposed new mood (must be in @ref kValidMoods).
    /// @return A @ref MutationRequest on success, or an error string.
    ValidationResult validate_update_mood(const std::string &npc_id, const std::string &mood) const;

    /// @brief Validate an update_trust request.
    ///
    /// @details The delta is pre-clamped so the resulting trust remains in
    /// [-100, 100].  The returned @ref MutationRequest contains the clamped delta.
    ///
    /// @param npc_id The NPC whose trust would change.
    /// @param delta  Signed change in trust.
    /// @return A @ref MutationRequest on success, or an error string.
    ValidationResult validate_update_trust(const std::string &npc_id, int delta) const;

    /// @brief Validate a move_npc request.
    /// @param npc_id      The NPC to move.
    /// @param location_id The destination location ID (must exist in the world).
    /// @return A @ref MutationRequest on success, or an error string.
    ValidationResult validate_move_npc(const std::string &npc_id,
                                       const std::string &location_id) const;

    /// @brief Validate a reveal_knowledge request.
    ///
    /// @details The fact must be in the NPC's knowledge set; this prevents
    /// the LLM from inventing facts.
    ///
    /// @param npc_id  The NPC revealing the fact.
    /// @param fact_id The fact ID to reveal.
    /// @return A @ref MutationRequest on success, or an error string.
    ValidationResult validate_reveal_knowledge(const std::string &npc_id,
                                               const std::string &fact_id) const;

    /// @brief Validate an add_memory request.
    ///
    /// @details Importance is clamped to [1, 10].
    ///
    /// @param npc_id     The NPC forming the memory.
    /// @param summary    Non-empty summary text.
    /// @param importance Salience score (clamped to [1, 10]).
    /// @return A @ref MutationRequest on success, or an error string.
    ValidationResult validate_add_memory(const std::string &npc_id, const std::string &summary,
                                         int importance) const;

    /// @brief Validate a set_flag request.
    ///
    /// @details The flag ID must already exist in @ref World::flags.
    ///
    /// @param flag_id The flag to set (must exist).
    /// @param value   The new boolean value.
    /// @return A @ref MutationRequest on success, or an error string.
    ValidationResult validate_set_flag(const std::string &flag_id, bool value) const;

    // -----------------------------------------------------------------------
    // Register methods: validate + enqueue on success
    // -----------------------------------------------------------------------
    // All register_* methods return std::nullopt on success or an error string
    // on validation failure.  On success, a MutationRequest is appended to the
    // pending queue.

    /// @brief Validate and enqueue a give_item mutation.
    /// @return @c std::nullopt on success; error string on failure.
    std::optional<std::string> register_give_item(const std::string &npc_id,
                                                  const std::string &item_id);

    /// @brief Validate and enqueue a take_item mutation.
    /// @return @c std::nullopt on success; error string on failure.
    std::optional<std::string> register_take_item(const std::string &npc_id,
                                                  const std::string &item_id);

    /// @brief Validate and enqueue an update_mood mutation.
    /// @return @c std::nullopt on success; error string on failure.
    std::optional<std::string> register_update_mood(const std::string &npc_id,
                                                    const std::string &mood);

    /// @brief Validate and enqueue an update_trust mutation.
    /// @return @c std::nullopt on success; error string on failure.
    std::optional<std::string> register_update_trust(const std::string &npc_id, int delta);

    /// @brief Validate and enqueue a move_npc mutation.
    /// @return @c std::nullopt on success; error string on failure.
    std::optional<std::string> register_move_npc(const std::string &npc_id,
                                                 const std::string &location_id);

    /// @brief Validate and enqueue a reveal_knowledge mutation.
    /// @return @c std::nullopt on success; error string on failure.
    std::optional<std::string> register_reveal_knowledge(const std::string &npc_id,
                                                         const std::string &fact_id);

    /// @brief Validate and enqueue an add_memory mutation.
    /// @return @c std::nullopt on success; error string on failure.
    std::optional<std::string> register_add_memory(const std::string &npc_id,
                                                   const std::string &summary, int importance);

    /// @brief Validate and enqueue a set_flag mutation.
    /// @return @c std::nullopt on success; error string on failure.
    std::optional<std::string> register_set_flag(const std::string &flag_id, bool value);

    /// @brief Handle the stringly Zoo-Keeper set_flag tool path.
    ///
    /// @details This keeps strict boolean parsing testable without requiring
    /// a live model-backed @c zoo::Agent invocation.
    ///
    /// @return "OK" on success, or an error string on validation failure.
    std::string handle_set_flag_tool(const std::string &flag_id, const std::string &value_str);

    /// @brief Handle the inspect_item tool path without requiring a zoo::Agent.
    ///
    /// @details Validates that the NPC has the item, then returns a formatted
    /// description including readable text if present.
    ///
    /// @param npc_id  The NPC inspecting the item.
    /// @param item_id The item to inspect.
    /// @return Item details on success, or an error string on failure.
    std::string handle_inspect_item_tool(const std::string &npc_id, const std::string &item_id);

    /// @brief Handle the take_item tool path without requiring a zoo::Agent.
    ///
    /// @details Validates and enqueues the take_item mutation, then returns
    /// a result string that includes item details (with readable text if present).
    ///
    /// @param npc_id  The NPC taking the item.
    /// @param item_id The item to take from the player.
    /// @return Item details prefixed with "OK." on success, or an error string.
    std::string handle_take_item_tool(const std::string &npc_id, const std::string &item_id);

    // -----------------------------------------------------------------------
    // Dialogue capture (non-mutating)
    // -----------------------------------------------------------------------

    /// @brief Capture a dialogue line from an NPC.
    ///
    /// @details Stores the @p npc_id / @p dialogue pair in the dialogue log.
    /// Non-mutating — does not push to the pending mutation queue.  The log
    /// is drained by @ref drain_dialogue_log.
    ///
    /// @param npc_id   ID of the NPC speaking.
    /// @param dialogue The dialogue text.
    void register_say(const std::string &npc_id, const std::string &dialogue);

    /// @brief Drain and return all captured dialogue lines.
    ///
    /// @details Clears the dialogue log after returning.
    /// @return Vector of (npc_id, dialogue) pairs in the order they were captured.
    std::vector<std::pair<std::string, std::string>> drain_dialogue_log();

    // -----------------------------------------------------------------------
    // Queue access
    // -----------------------------------------------------------------------

    /// @brief Read-only access to the pending mutation queue.
    ///
    /// @details Used by @ref GameEngine::process_pending_mutations to iterate
    /// the queue before clearing it.
    ///
    /// @return Reference to the internal pending mutation vector.
    const std::vector<MutationRequest> &pending_mutations() const;

    /// @brief Clear all pending mutations without applying them.
    void clear_pending();

    /// @brief Clear the dialogue log without processing it.
    void clear_dialogue_log();

    /// @brief Clear both the pending mutation queue and the dialogue log.
    void clear_all();

    // -----------------------------------------------------------------------
    // Zoo-Keeper integration
    // -----------------------------------------------------------------------

    /// @brief Set the NPC ID used by registered Zoo-Keeper tool lambdas.
    ///
    /// @details Tool lambdas capture @c this by pointer and read
    /// @c active_npc_id_ at call time.  This must be set before any tool
    /// lambda is invoked.
    ///
    /// @param npc_id The active NPC's ID.
    void set_active_npc_id(std::string npc_id);

    /// @brief Register all game tools on a @c zoo::Agent instance.
    ///
    /// @details Creates and registers a lambda for each state-changing game tool
    /// (give_item, take_item, update_mood, update_trust, move_self, reveal_knowledge,
    /// remember, set_flag).  Each lambda captures @c this by pointer and calls the
    /// corresponding tool handler or @c register_* method at inference time.
    ///
    /// @param agent   The Zoo-Keeper agent to register tools on.
    /// @param npc_id  The active NPC — forwarded to @ref set_active_npc_id.
    void register_tools(zoo::Agent &agent, const std::string &npc_id);

  private:
    const World &world_;                   ///< Read-only world reference used for validation.
    MutationSink sink_;                    ///< External mutation consumer (if set).
    std::vector<MutationRequest> pending_; ///< Fallback queue when no sink is provided.
    std::vector<std::pair<std::string, std::string>> dialogue_log_; ///< Captured NPC dialogue.
    std::string active_npc_id_; ///< NPC context for tool lambda callbacks.

    /// @brief Enqueue a mutation via the sink or the internal fallback vector.
    void emit(MutationRequest req);

    /// @brief Format an item's details for tool results.
    ///
    /// @details Returns a human-readable summary of the item including its name,
    /// description, and readable text (if any).  Used by take_item (automatic
    /// feedback on readable items) and inspect_item.
    ///
    /// @param item_id The item to describe.
    /// @return Formatted string with item details.
    std::string format_item_details(const std::string &item_id) const;

    /// @brief Return a policy error if the NPC is forbidden from calling a tool.
    ///
    /// @details Missing @c tool_policy defaults are expanded during load.
    /// An empty @c allowed_tools list means no tool permissions.
    std::optional<std::string> policy_tool_error(const std::string &npc_id,
                                                 std::string_view tool_name) const;

    /// @brief Return a policy error if an item ID is outside the NPC's scope.
    ///
    /// @details Empty scoped ID lists mean unrestricted within that scope.
    /// Policy failures are reported separately from missing world references so
    /// creators can distinguish permissions from typos.
    std::optional<std::string> policy_item_error(const std::string &npc_id,
                                                 const std::string &item_id) const;

    /// @brief Return a policy error if a fact ID is outside the NPC's scope.
    std::optional<std::string> policy_fact_error(const std::string &npc_id,
                                                 const std::string &fact_id) const;

    /// @brief Return a policy error if a flag ID is outside the NPC's scope.
    std::optional<std::string> policy_flag_error(const std::string &npc_id,
                                                 const std::string &flag_id) const;

    /// @brief Return a policy error if a location ID is outside the NPC's scope.
    std::optional<std::string> policy_location_error(const std::string &npc_id,
                                                     const std::string &location_id) const;

    /// @brief Test whether an NPC ID exists in the world.
    bool npc_exists(const std::string &npc_id) const;

    /// @brief Test whether an NPC currently holds an item.
    bool npc_has_item(const std::string &npc_id, const std::string &item_id) const;

    /// @brief Test whether the player currently holds an item.
    bool player_has_item(const std::string &item_id) const;

    /// @brief Test whether an item exists and is marked as key-item protected.
    bool is_key_item(const std::string &item_id) const;

    /// @brief Test whether an item ID exists in the world registry.
    bool item_exists(const std::string &item_id) const;

    /// @brief Test whether a location ID exists in the world.
    bool location_exists(const std::string &location_id) const;

    /// @brief Test whether a flag ID exists in the world.
    bool flag_exists(const std::string &flag_id) const;

    /// @brief Test whether a mood belongs to the canonical NPC mood vocabulary.
    bool is_valid_mood(const std::string &mood) const;
};

} // namespace chronicle
