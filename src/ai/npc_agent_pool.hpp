/**
 * @file npc_agent_pool.hpp
 * @brief Shared LLM agent management for NPC interactions.
 *
 * @details Chronicle uses a single shared @c zoo::Agent instance (the "Option A"
 * architecture described in the design documents) rather than one agent per
 * NPC.  @ref NpcAgentPool manages that instance and enforces the invariant that
 * only one NPC may use the agent at a time.
 *
 * Access is granted through a move-only @ref NpcAgentHandle RAII type.
 * Acquiring a handle clears the agent's conversation history and sets the
 * active NPC context; releasing the handle (via destructor or move-assignment)
 * clears history again and returns the agent to an available state.
 *
 * ### Usage pattern
 * @code
 * auto handle = pool.acquire(npc_id);
 * handle->register_tools(tool_registry, npc_id);
 * handle->set_system_prompt(system_prompt);
 * auto result = handle->chat_streaming(user_msg, on_token, poll_fn);
 * // handle goes out of scope → agent released back to pool
 * @endcode
 */

#pragma once
#include "ai/agent_interface.hpp"

#include <memory>
#include <string>

namespace chronicle {

struct Config;
class NpcAgentPool;

/// @brief RAII handle providing exclusive access to the shared agent.
///
/// @details Move-only.  Wraps an @ref AgentInterface pointer borrowed from
/// @ref NpcAgentPool.  The destructor calls @ref NpcAgentPool::release,
/// which clears conversation history and marks the agent as available.
class NpcAgentHandle {
public:
    /// @brief Construct a handle that owns the agent for @p npc_id.
    ///
    /// @param agent   Pointer to the agent to borrow (must not be @c nullptr).
    /// @param pool    Owning pool; @c release() is called on destruction.
    /// @param npc_id  ID of the NPC this handle was acquired for.
    NpcAgentHandle(AgentInterface* agent, NpcAgentPool* pool, std::string npc_id);

    /// Releases the agent back to the pool by calling @ref NpcAgentPool::release.
    ~NpcAgentHandle();

    /// @brief Move constructor.  Transfers ownership; source becomes null.
    NpcAgentHandle(NpcAgentHandle&& other) noexcept;

    /// @brief Move assignment.  Releases any current ownership before acquiring the new one.
    NpcAgentHandle& operator=(NpcAgentHandle&& other) noexcept;

    NpcAgentHandle(const NpcAgentHandle&) = delete;
    NpcAgentHandle& operator=(const NpcAgentHandle&) = delete;

    /// Pointer-like access to the underlying @ref AgentInterface.
    AgentInterface* operator->() { return agent_; }
    /// Pointer-like access to the underlying @ref AgentInterface.
    AgentInterface& operator*() { return *agent_; }
    /// @overload
    const AgentInterface* operator->() const { return agent_; }
    /// @overload
    const AgentInterface& operator*() const { return *agent_; }

    /// @return The NPC ID this handle was acquired for.
    const std::string& npc_id() const { return npc_id_; }

private:
    AgentInterface* agent_ = nullptr; ///< Borrowed pointer; not owned.
    NpcAgentPool* pool_ = nullptr;    ///< Pool to release back to.
    std::string npc_id_;              ///< NPC context for this borrow.
};

/// @brief Manages a single shared agent instance (Option A — shared agent).
///
/// @details Owns one @ref AgentInterface and enforces mutual exclusion so that
/// at most one @ref NpcAgentHandle exists at any time.  If a second acquire is
/// attempted while the agent is in use, @ref acquire throws.
///
/// The production path is @ref from_config, which creates a real
/// @c zoo::Agent via the @ref ZooAgentAdapter.  For tests, the
/// @ref NpcAgentPool(std::unique_ptr<AgentInterface>) constructor accepts a
/// mock implementation.
class NpcAgentPool {
public:
    /// @brief Test-injection constructor: takes a pre-built agent.
    ///
    /// @param agent The @ref AgentInterface implementation to manage.
    explicit NpcAgentPool(std::unique_ptr<AgentInterface> agent);

    /// @brief Production factory: create a pool from a loaded @ref Config.
    ///
    /// @details Constructs a @c zoo::Agent using the model path and inference
    /// parameters from @p config, wraps it in a @ref ZooAgentAdapter, and
    /// returns an @ref NpcAgentPool owning it.
    ///
    /// @param config Runtime configuration providing model path and parameters.
    /// @return A new @ref NpcAgentPool ready for use.
    /// @throws std::runtime_error if @c config.model_path is empty or if the
    ///         Zoo-Keeper agent creation fails.
    static NpcAgentPool from_config(const Config &config);

    /// @brief Acquire exclusive access to the shared agent for a given NPC.
    ///
    /// @details Clears the agent's conversation history before returning the
    /// handle so previous NPC context does not leak into the new conversation.
    ///
    /// @param npc_id The ID of the NPC about to converse with the player.
    /// @return An @ref NpcAgentHandle that exclusively borrows the agent.
    /// @throws std::runtime_error if the agent is already in use.
    NpcAgentHandle acquire(const std::string& npc_id);

private:
    std::unique_ptr<AgentInterface> agent_; ///< The owned agent instance.
    bool in_use_ = false;                   ///< Whether a handle is currently active.

    /// Release the agent after a handle is destroyed.
    void release();

    friend class NpcAgentHandle;
};

} // namespace chronicle
