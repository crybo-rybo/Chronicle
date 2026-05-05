/**
 * @file game_engine.hpp
 * @brief Root orchestrator for the Chronicle game loop.
 *
 * @details @ref GameEngine is the top-level controller.  It owns every major
 * subsystem (world state, renderer, AI layer, save system) and is the
 * **sole point of mutation** for the @ref World.  All state changes —
 * including those enqueued by LLM tool call handlers — flow through
 * @ref process_pending_mutations, which validates and applies them in a
 * deterministic order at the end of each turn.
 *
 * ### Game loop
 * @ref run enters a blocking loop:
 * 1. Drain any buffered streaming tokens and render them.
 * 2. Display the input prompt appropriate for the current @ref GamePhase.
 * 3. Read one line of player input.
 * 4. Parse it with @ref CommandParser.
 * 5. Dispatch to @ref handle_command (which may invoke the AI layer).
 * 6. Successful significant actions run the post-turn pipeline.
 * 7. Repeat until @c running_ is @c false.
 *
 * ### Design invariant
 * The AI layer (@ref NpcAgentPool, @ref ToolRegistry, @ref ResponseHandler)
 * may read @ref World freely but must never write to it directly.  All AI-
 * initiated state changes are expressed as @ref MutationRequest objects queued
 * in @ref ToolRegistry and applied by the engine after inference completes.
 */

#pragma once

#include "ai/npc_agent_pool.hpp"
#include "ai/response_handler.hpp"
#include "ai/tool_registry.hpp"
#include "engine/command_parser.hpp"
#include "engine/game_phase.hpp"
#include "engine/mutation_request.hpp"
#include "engine/token_queue.hpp"
#include "entities/config.hpp"
#include "entities/world.hpp"
#include "entities/world_loader.hpp"
#include "persistence/save_system.hpp"
#include "rendering/renderer.hpp"

#include <memory>
#include <optional>
#include <string>

namespace chronicle {

/// @brief Top-level game orchestrator.
///
/// @details Owns all subsystems and implements the main game loop.  The class
/// is intentionally not copyable or movable after construction.
class GameEngine {
  public:
    /// @brief Construct from explicit scenario world file paths.
    GameEngine(const std::string &config_path, const WorldFileSet &world_files,
               std::unique_ptr<Renderer> renderer,
               std::unique_ptr<NpcAgentPool> agent_pool = nullptr);

    /// @brief Run the game until the player quits or the game ends.
    ///
    /// @details This is a blocking call that owns the main thread for the
    /// duration of the session.  Returns when @c running_ becomes @c false.
    void run();

    /// @brief Dispatch a @ref ParsedCommand to the appropriate handler.
    ///
    /// @details Handles movement, inventory, examination, save/load, talk,
    /// and dialogue routing.  Exposed as @c public for unit testing.
    ///
    /// @param cmd The parsed command to execute.
    void handle_command(const ParsedCommand &cmd);

    /// @brief Drain the pending mutation queue and apply all validated changes.
    ///
    /// @details Iterates the engine-owned pending queue, applies each request
    /// via @ref apply_mutation, then narrates successful mutations via
    /// @ref ResponseHandler.  Also detects if the active conversation NPC has
    /// left the current location and ends the conversation if so.
    ///
    /// Exposed as @c public for unit testing.
    void process_pending_mutations();

    /// @brief Read-only access to the current world state (for testing).
    /// @return A const reference to the world owned by this engine.
    const World &world() const { return world_; }

    /// @brief Query the current game phase (for testing).
    /// @return The current @ref GamePhase.
    GamePhase phase() const { return phase_; }

    /// @brief Execute a single dialogue turn with the active NPC.
    ///
    /// @details Injects fresh dynamic context, builds the user turn, streams
    /// the agent response through the renderer, and calls
    /// @ref process_pending_mutations after inference completes.  If the agent
    /// pool is not initialised, a stub message is rendered instead.
    ///
    /// @param npc_id The ID of the NPC to converse with.
    /// @param input  The player's dialogue text.
    void handle_dialogue(const std::string &npc_id, const std::string &input);

  private:
    Config config_;                      ///< Loaded runtime configuration.
    World world_;                        ///< Authoritative game state.
    CommandParser parser_;               ///< Input parser (phase-aware).
    std::unique_ptr<Renderer> renderer_; ///< Output renderer.
    SaveSystem save_system_;             ///< Save/load manager.
    TokenQueue token_queue_;             ///< Streaming token bridge.

    /// Agent pool — may be @c nullptr if no model is configured.
    std::unique_ptr<NpcAgentPool> agent_pool_;
    /// Tool registry — validates and routes NPC mutations to @c pending_mutations_.
    std::unique_ptr<ToolRegistry> tool_registry_;
    /// Response handler — narrates mutations and forwards tokens.
    std::unique_ptr<ResponseHandler> response_handler_;
    /// Authoritative pending mutation queue — drained by @ref process_pending_mutations.
    std::vector<MutationRequest> pending_mutations_;

    bool running_ = true;                  ///< Set to @c false to exit the game loop.
    GamePhase phase_ = GamePhase::Playing; ///< Current game phase.
    std::optional<std::string> current_conversation_npc_id_; ///< NPC ID for active conversation.
    std::optional<NpcAgentHandle>
        active_conversation_handle_; ///< Agent handle for active conversation.

    /// Re-render the current location scene.
    void render_current_scene();

    /// Display the player's inventory.
    void render_inventory();

    /// @brief Flush all queued tokens to the renderer.
    ///
    /// @return @c true if at least one token was rendered.
    bool drain_token_queue();

    /// @brief Test whether an item is visible in the current location.
    /// @param item_id The item to check.
    bool player_can_see_item(const std::string &item_id) const;

    /// @brief Test whether an NPC is present in the current location.
    /// @param npc_id The NPC to check.
    bool player_can_see_npc(const std::string &npc_id) const;

    /// @brief Find an NPC in the current location by a partial name match.
    ///
    /// @param query Case-insensitive partial name or ID.
    /// @return The NPC's ID if found, @c std::nullopt otherwise.
    std::optional<std::string> find_visible_npc_id(const std::string &query) const;

    /// @brief Find an accessible item by a partial name match.
    ///
    /// @details Searches the player's inventory first, then the current
    /// location (skipping hidden items).
    ///
    /// @param query Case-insensitive partial name or ID.
    /// @return The item's ID if found, @c std::nullopt otherwise.
    std::optional<std::string> find_accessible_item_id(const std::string &query) const;

    /// @brief Parse a save/load slot number from a string argument.
    ///
    /// @details Returns @c 1 if the string is empty (default slot).  Returns
    /// @c std::nullopt if the string is not a positive integer.
    ///
    /// @param slot_text The text to parse, typically @c ParsedCommand::primary_arg.
    std::optional<int> parse_slot(const std::string &slot_text) const;

    /// End the active conversation and return to the Playing phase.
    void leave_conversation();

    /// @brief Initialize a conversation with an NPC.
    ///
    /// @details Acquires the agent from the pool, registers tools, builds and
    /// sets the static system prompt, and transitions to @ref GamePhase::InConversation.
    ///
    /// @param npc_id The ID of the NPC to converse with.
    /// @return @c true if the conversation was successfully started.
    bool start_conversation(const std::string &npc_id);

    /// @brief Test whether the given input is a natural-language exit phrase.
    ///
    /// @details Recognises phrases such as @c "bye", @c "goodbye",
    /// @c "leave", @c "exit conversation", and @c "leave conversation".
    ///
    /// @param input The raw player input to test.
    bool is_conversation_exit(std::string_view input) const;

    /// Apply pending mutations, advance time once, and run post-turn systems.
    void run_post_turn_pipeline();

    /// Evaluate eligible scripted events after a significant turn.
    void evaluate_scripted_events();
};

} // namespace chronicle
