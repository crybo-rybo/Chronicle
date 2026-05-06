/**
 * @file zoo_agent_adapter.hpp
 * @brief Concrete @ref AgentInterface implementation wrapping a Zoo-Keeper agent.
 *
 * @details @ref ZooAgentAdapter is the production implementation of
 * @ref AgentInterface.  It wraps a @c zoo::Agent and translates Chronicle's
 * abstract interface calls into Zoo-Keeper API calls.
 *
 * ### Streaming and polling
 * @ref chat_streaming launches inference via @c zoo::Agent::chat, which
 * runs asynchronously on Zoo-Keeper's internal thread.  The adapter polls the
 * handle in a 10 ms sleep loop, invoking the @ref PollCallback each iteration
 * so the main thread can drain the @ref TokenQueue and update the display in
 * near-real-time.
 *
 * ### Tool registration
 * Tool registration is idempotent per registry instance — tools are only
 * registered on the underlying @c zoo::Agent once per @ref ToolRegistry
 * pointer.  This avoids redundant registration when the same NPC is accessed
 * across multiple turns.
 */

#pragma once
#include "ai/agent_interface.hpp"
#include <memory>

#ifndef CHRONICLE_ENABLE_ZOO
#define CHRONICLE_ENABLE_ZOO 1
#endif

#if CHRONICLE_ENABLE_ZOO
#include <zoo/agent.hpp>
#endif

namespace chronicle {

/// @brief Default per-request inference timeout in milliseconds (two minutes).
inline constexpr int kDefaultInferenceTimeoutMs = 120000;

#if CHRONICLE_ENABLE_ZOO
/// @brief Wraps a @c zoo::Agent to satisfy the @ref AgentInterface contract.
class ZooAgentAdapter : public AgentInterface {
  public:
    /// @brief Construct from an existing Zoo-Keeper agent.
    ///
    /// @param agent                A non-null @c zoo::Agent to wrap.
    /// @param inference_timeout_ms Per-request timeout in milliseconds; @c 0 disables.
    /// @throws std::invalid_argument if @p agent is @c nullptr.
    explicit ZooAgentAdapter(std::unique_ptr<zoo::Agent> agent,
                             int inference_timeout_ms = kDefaultInferenceTimeoutMs);

    /// @brief Set the system prompt on the underlying @c zoo::Agent.
    void set_system_prompt(std::string_view prompt) override;

    /// @brief Inject a mid-conversation system message via @c zoo::Agent::add_system_message.
    void add_system_message(std::string_view message) override;

    /// @brief Clear the conversation history on the underlying @c zoo::Agent.
    void clear_history() override;

    /// @brief Delegate @c is_running check to the underlying @c zoo::Agent.
    bool is_running() const noexcept override;

    /// @brief Register game tools on the underlying agent via @ref ToolRegistry.
    ///
    /// @details Calls @ref ToolRegistry::set_active_npc_id and, if this is a
    /// new registry instance, @ref ToolRegistry::register_tools.  Skips
    /// re-registration if the same registry pointer is presented again.
    ///
    /// @param tool_registry The tool registry to register tools from.
    /// @param npc_id        The NPC whose tool context should be active.
    void register_tools(ToolRegistry &tool_registry, const std::string &npc_id) override;

    /// @brief Invoke @c zoo::Agent::chat and poll until completion.
    ///
    /// @details Calls @c zoo::Agent::chat with @p on_token as the streaming
    /// callback.  Polls the inference handle in a 10 ms sleep loop, calling
    /// @p poll each iteration.  After the handle reports ready, drains any
    /// final tokens via @p poll and calls @c handle.await_result().
    ///
    /// @param user_message The player's message.
    /// @param on_token     Callback invoked on the inference thread per token.
    /// @param poll         Callback invoked on the calling thread while waiting.
    /// @return @ref AgentChatResult indicating success or failure.
    AgentChatResult chat_streaming(std::string_view user_message, TokenCallback on_token,
                                   PollCallback poll) override;

    /// @brief Direct access to the underlying @c zoo::Agent.
    /// @return Reference to the owned @c zoo::Agent.
    zoo::Agent &agent() { return *agent_; }

    /// @overload
    const zoo::Agent &agent() const { return *agent_; }

  private:
    std::unique_ptr<zoo::Agent> agent_;                ///< The wrapped Zoo-Keeper agent.
    ToolRegistry *registered_tool_registry_ = nullptr; ///< Tracks the last registered registry.
    int inference_timeout_ms_ = kDefaultInferenceTimeoutMs; ///< Per-request timeout; 0 disables.
};
#endif

} // namespace chronicle
