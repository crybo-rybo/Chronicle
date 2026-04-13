/**
 * @file npc_agent_pool.cpp
 * @brief Implementation of @ref NpcAgentPool and @ref NpcAgentHandle.
 *
 * @details Implements the RAII handle move semantics and the pool acquire/release
 * cycle, as well as the production @ref NpcAgentPool::from_config factory that
 * creates a @c zoo::Agent from runtime configuration parameters.
 */

#include "ai/npc_agent_pool.hpp"
#include "ai/zoo_agent_adapter.hpp"
#include "entities/config.hpp"
#include <stdexcept>
#include <zoo/agent.hpp>

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

NpcAgentPool NpcAgentPool::from_config(const Config &config) {
    if (config.model_path.empty()) {
        throw std::runtime_error(
            "NpcAgentPool::from_config: model_path is empty. "
            "Set model_path in config/default.json to a valid GGUF model file.");
    }

    zoo::ModelConfig model_config{
        .model_path = config.model_path,
        .context_size = config.context_size,
        .n_gpu_layers = config.n_gpu_layers,
    };

    zoo::GenerationOptions gen_opts;
    gen_opts.sampling.temperature = static_cast<float>(config.temperature);
    gen_opts.max_tokens = config.max_response_tokens;

    auto result = zoo::Agent::create(model_config, {}, gen_opts);
    if (!result) {
        throw std::runtime_error("NpcAgentPool::from_config: failed to create zoo::Agent: " +
                                 result.error().to_string());
    }

    return NpcAgentPool(std::make_unique<ZooAgentAdapter>(std::move(*result)));
}

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
