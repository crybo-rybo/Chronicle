/**
 * @file agent_interface.hpp
 * @brief Abstract interface for the LLM agent used in NPC dialogue.
 *
 * @details @ref AgentInterface decouples Chronicle's game logic from the
 * concrete Zoo-Keeper @c zoo::Agent implementation.  This separation allows:
 *
 * - **Unit testing** without loading a real model — tests inject a mock
 *   @ref AgentInterface via @ref NpcAgentPool's test-injection constructor.
 * - **Future flexibility** — an alternative inference backend can be swapped
 *   in by implementing this interface without touching the engine layer.
 *
 * The concrete production implementation is @ref ZooAgentAdapter.
 */

#pragma once
#include <functional>
#include <string>
#include <string_view>

namespace chronicle {

class ToolRegistry;

/// @brief Result of a single @ref AgentInterface::chat_streaming call.
///
/// @details A failed result may still follow partial token delivery; callers
/// must treat streamed text as display-only until validated mutations have
/// been applied by the engine.  Implementations should prefer returning
/// @c success == false over throwing for recoverable inference errors.
struct AgentChatResult {
    bool success = false;      ///< @c true if inference completed without error.
    std::string error_message; ///< Human-readable error description; empty on success.
};

/// @brief Thin abstract base class exposing the agent lifecycle methods Chronicle requires.
///
/// @details Concrete implementations wrap an LLM agent and translate Chronicle
/// calls into the underlying framework's API.  The interface intentionally
/// exposes only the subset of functionality the game needs.
class AgentInterface {
  public:
    /// @brief Callback invoked on the inference thread for each generated token.
    ///
    /// @details Implementations should be non-blocking.  The callback receives
    /// a @c string_view into token memory that is valid only for the duration
    /// of the call.
    using TokenCallback = std::function<void(std::string_view)>;

    /// @brief Callback invoked periodically during inference for main-thread work.
    ///
    /// @details @ref ZooAgentAdapter calls this in its polling loop so the
    /// main thread can drain the @ref TokenQueue and update the display while
    /// waiting for inference to complete.
    using PollCallback = std::function<void()>;

    virtual ~AgentInterface() = default;

    /// @brief Set or replace the system prompt used for the next conversation.
    ///
    /// @details Called before each NPC interaction to inject the NPC persona
    /// built by @ref PromptBuilder.
    ///
    /// @param prompt The new system prompt text.
    virtual void set_system_prompt(std::string_view prompt) = 0;

    /// @brief Inject a mid-conversation system message into the agent's history.
    ///
    /// @details Inserts @p message under the @c system role so the model sees
    /// it as authoritative context rather than user/assistant content.  Used to
    /// deliver the per-turn dynamic state block (@c [Current state]) without
    /// polluting the user/assistant alternation pattern that can trigger
    /// hallucinated turn continuations.
    ///
    /// @param message The system context text to inject.
    virtual void add_system_message(std::string_view message) = 0;

    /// @brief Clear the agent's conversation history.
    ///
    /// @details Called by @ref NpcAgentPool before and after each interaction
    /// to prevent context leakage between NPCs.
    virtual void clear_history() = 0;

    /// @brief Test whether the agent is currently running an inference call.
    /// @return @c true if an inference is in progress.
    virtual bool is_running() const noexcept = 0;

    /// @brief Register game tools on the agent for the given NPC context.
    ///
    /// @details Delegates to @ref ToolRegistry::register_tools, wiring up
    /// the Zoo-Keeper tool-call callbacks.  Also updates the active NPC ID
    /// in the registry.
    ///
    /// @param tool_registry The registry that validates and enqueues mutations.
    /// @param npc_id        The ID of the NPC whose tools should be active.
    virtual void register_tools(ToolRegistry &tool_registry, const std::string &npc_id) = 0;

    /// @brief Send a user message to the agent and stream the response.
    ///
    /// @details Blocks (with periodic @p poll callbacks) until inference
    /// completes.  Each generated token is delivered to @p on_token on the
    /// inference thread.  Tool calls may enqueue validated mutations through
    /// @ref ToolRegistry, but no agent implementation may directly write to
    /// @ref World.
    ///
    /// @param user_message The player's message to send to the agent.
    /// @param on_token     Called on the inference thread for each token.
    /// @param poll         Called periodically on the calling thread while waiting.
    /// @return An @ref AgentChatResult indicating success or failure.  On
    ///         failure, callers should keep non-LLM gameplay usable and avoid
    ///         applying unvalidated partial state.
    virtual AgentChatResult chat_streaming(std::string_view user_message, TokenCallback on_token,
                                           PollCallback poll) = 0;
};

} // namespace chronicle
