#pragma once
#include "ai/agent_interface.hpp"

#include <memory>
#include <string>

namespace chronicle {

struct Config;
class NpcAgentPool;

/// RAII handle that exclusively borrows an agent for a single NPC interaction.
/// Move-only — destructor releases the agent back to the pool.
class NpcAgentHandle {
public:
    NpcAgentHandle(AgentInterface* agent, NpcAgentPool* pool, std::string npc_id);
    ~NpcAgentHandle();

    NpcAgentHandle(NpcAgentHandle&& other) noexcept;
    NpcAgentHandle& operator=(NpcAgentHandle&& other) noexcept;

    NpcAgentHandle(const NpcAgentHandle&) = delete;
    NpcAgentHandle& operator=(const NpcAgentHandle&) = delete;

    AgentInterface* operator->() { return agent_; }
    AgentInterface& operator*() { return *agent_; }
    const AgentInterface* operator->() const { return agent_; }
    const AgentInterface& operator*() const { return *agent_; }
    const std::string& npc_id() const { return npc_id_; }

private:
    AgentInterface* agent_ = nullptr;
    NpcAgentPool* pool_ = nullptr;
    std::string npc_id_;
};

/// Manages a single shared agent instance (Option A — shared agent).
/// Only one NPC may hold the agent at a time; double-acquire throws.
class NpcAgentPool {
public:
    /// Test-injection constructor: takes a pre-built agent.
    explicit NpcAgentPool(std::unique_ptr<AgentInterface> agent);

    /// Production factory: creates pool with a real zoo::Agent from Config.
    /// @throws std::runtime_error on empty model_path or agent creation failure.
    static NpcAgentPool from_config(const Config &config);

    /// Acquires exclusive access to the agent for the given NPC.
    /// Clears conversation history before returning the handle.
    /// @throws std::runtime_error if the agent is already in use.
    NpcAgentHandle acquire(const std::string& npc_id);

private:
    std::unique_ptr<AgentInterface> agent_;
    bool in_use_ = false;

    void release();
    friend class NpcAgentHandle;
};

} // namespace chronicle
