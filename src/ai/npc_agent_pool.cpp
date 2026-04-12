#include "ai/npc_agent_pool.hpp"

namespace chronicle {

// ---------------------------------------------------------------------------
// NpcAgentHandle
// ---------------------------------------------------------------------------

NpcAgentHandle::NpcAgentHandle(AgentInterface* agent, NpcAgentPool* pool, std::string npc_id)
    : agent_(agent), pool_(pool), npc_id_(std::move(npc_id)) {}

NpcAgentHandle::~NpcAgentHandle() {
    if (pool_) {
        pool_->release();
    }
}

NpcAgentHandle::NpcAgentHandle(NpcAgentHandle&& other) noexcept
    : agent_(other.agent_), pool_(other.pool_), npc_id_(std::move(other.npc_id_)) {
    other.agent_ = nullptr;
    other.pool_ = nullptr;
}

NpcAgentHandle& NpcAgentHandle::operator=(NpcAgentHandle&& other) noexcept {
    if (this != &other) {
        if (pool_) {
            pool_->release();
        }
        agent_ = other.agent_;
        pool_ = other.pool_;
        npc_id_ = std::move(other.npc_id_);
        other.agent_ = nullptr;
        other.pool_ = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// NpcAgentPool
// ---------------------------------------------------------------------------

NpcAgentPool::NpcAgentPool(std::unique_ptr<AgentInterface> agent) : agent_(std::move(agent)) {}

NpcAgentHandle NpcAgentPool::acquire(const std::string& npc_id) {
    if (in_use_) {
        throw std::runtime_error("NpcAgentPool: agent is already in use");
    }
    in_use_ = true;
    agent_->clear_history();
    return NpcAgentHandle(agent_.get(), this, npc_id);
}

void NpcAgentPool::release() {
    agent_->clear_history();
    in_use_ = false;
}

} // namespace chronicle
