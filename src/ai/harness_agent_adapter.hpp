/**
 * @file harness_agent_adapter.hpp
 * @brief Concrete @ref AgentInterface implementation wrapping a zoo-keeper-harness agent.
 *
 * @details @ref HarnessAgentAdapter is the production implementation of
 * @ref AgentInterface.  It wraps a @c zoo::Agent and translates Chronicle's
 * abstract interface calls into zoo-keeper-harness API calls.
 *
 * ### System context
 * The harness keeps a single system message at the front of history. Chronicle
 * still models static NPC persona and dynamic per-turn state separately, so
 * this adapter stores both pieces and rewrites the harness system prompt before
 * each run.
 *
 * ### Tool context
 * Harness tools are registered on @c zoo::Agent::Builder before construction.
 * @ref register_tools therefore only selects the active NPC ID on the already
 * captured @ref ToolRegistry.
 */

#pragma once
#include "ai/agent_interface.hpp"
#include "ai/harness_compat.hpp"
#include "entities/config.hpp"
#include <memory>
#include <string>

#if CHRONICLE_ENABLE_HARNESS
#include <zoo/Agent.hpp>
#endif

namespace chronicle {

#if CHRONICLE_ENABLE_HARNESS
/// @brief Wraps a @c zoo::Agent to satisfy the @ref AgentInterface contract.
class HarnessAgentAdapter : public AgentInterface {
  public:
    /// @brief Construct from an existing harness agent and registered tool registry.
    ///
    /// @param agent                A non-null @c zoo::Agent to wrap.
    /// @param tool_registry        Registry captured by the agent's registered tool handlers.
    /// @param inference_timeout_ms Per-request timeout in milliseconds; @c 0 disables.
    /// @throws std::invalid_argument if @p agent is @c nullptr.
    HarnessAgentAdapter(std::unique_ptr<zoo::Agent> agent, ToolRegistry &tool_registry,
                        int inference_timeout_ms = kDefaultInferenceTimeoutMs);

    /// @brief Set the static NPC system prompt.
    void set_system_prompt(std::string_view prompt) override;

    /// @brief Refresh the dynamic per-turn context in the combined system prompt.
    void add_system_message(std::string_view message) override;

    /// @brief Clear the conversation history and cached system prompt state.
    void clear_history() override;

    /// @brief Return whether this adapter is inside a blocking harness run.
    bool is_running() const noexcept override;

    /// @brief Register game tools on the underlying agent via @ref ToolRegistry.
    ///
    /// @details Calls @ref ToolRegistry::set_active_npc_id on the registry that
    /// was captured when the harness agent was constructed.
    ///
    /// @param tool_registry The tool registry to register tools from.
    /// @param npc_id        The NPC whose tool context should be active.
    void register_tools(ToolRegistry &tool_registry, const std::string &npc_id) override;

    /// @brief Invoke @c zoo::Agent::run and stream tokens through Chronicle.
    ///
    /// @param user_message The player's message.
    /// @param on_token     Callback invoked on the inference thread per token.
    /// @param poll         Callback invoked on the calling thread while waiting.
    /// @return @ref AgentChatResult indicating success or failure.
    AgentChatResult chat_streaming(std::string_view user_message, TokenCallback on_token,
                                   PollCallback poll) override;

  private:
    void apply_combined_system_prompt();

    std::unique_ptr<zoo::Agent> agent_;     ///< The wrapped harness agent.
    ToolRegistry *tool_registry_ = nullptr; ///< Registry captured by the harness tool handlers.
    std::string static_system_prompt_;
    std::string dynamic_system_context_;
    int inference_timeout_ms_ = kDefaultInferenceTimeoutMs; ///< Per-request timeout; 0 disables.
    bool running_ = false;
};
#endif

} // namespace chronicle
