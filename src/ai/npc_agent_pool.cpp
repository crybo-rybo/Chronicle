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
#include "diagnostics/logger.hpp"
#include "entities/config.hpp"
#include <stdexcept>
#include <zoo/agent.hpp>
#include <zoo/log.hpp>

namespace chronicle {

namespace {

logging::Level to_chronicle_level(zoo::LogLevel level) {
    switch (level) {
    case zoo::LogLevel::Debug:
        return logging::Level::Debug;
    case zoo::LogLevel::Info:
        return logging::Level::Info;
    case zoo::LogLevel::Warning:
        return logging::Level::Warning;
    case zoo::LogLevel::Error:
        return logging::Level::Error;
    }
    return logging::Level::Info;
}

void zoo_log_bridge(zoo::LogLevel level, const char *message, void *) {
    logging::write(to_chronicle_level(level), "zoo", message ? message : "");
}

void install_zoo_log_bridge_if_enabled() {
    static bool installed = false;
    if (!logging::is_enabled() || installed) {
        return;
    }

    zoo::set_log_callback(zoo_log_bridge);
    installed = true;
    logging::write(logging::Level::Info, "zoo", "installed Zoo-Keeper log bridge");
}

} // namespace

// ---------------------------------------------------------------------------
// NpcAgentHandle
// ---------------------------------------------------------------------------

NpcAgentHandle::NpcAgentHandle(AgentInterface *agent, NpcAgentPool *pool, std::string npc_id)
    : agent_(agent), pool_(pool), npc_id_(std::move(npc_id)) {}

NpcAgentHandle::~NpcAgentHandle() {
    if (pool_) {
        pool_->release();
    }
}

NpcAgentHandle::NpcAgentHandle(NpcAgentHandle &&other) noexcept
    : agent_(other.agent_), pool_(other.pool_), npc_id_(std::move(other.npc_id_)) {
    other.agent_ = nullptr;
    other.pool_ = nullptr;
}

NpcAgentHandle &NpcAgentHandle::operator=(NpcAgentHandle &&other) noexcept {
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
            "Set model_path in the scenario config file to a valid GGUF model file.");
    }

    install_zoo_log_bridge_if_enabled();
    logging::write(logging::Level::Info, "ai",
                   "creating zoo agent model_path=" + config.model_path +
                       " context_size=" + std::to_string(config.context_size) +
                       " n_gpu_layers=" + std::to_string(config.n_gpu_layers) +
                       " max_response_tokens=" + std::to_string(config.max_response_tokens) +
                       " temperature=" + std::to_string(config.temperature) +
                       " max_tool_iterations=" + std::to_string(config.max_tool_iterations) +
                       " inference_timeout_ms=" + std::to_string(config.inference_timeout_ms));

    zoo::ModelConfig model_config{
        .model_path = config.model_path,
        .context_size = config.context_size,
        .n_gpu_layers = config.n_gpu_layers,
    };

    zoo::GenerationOptions gen_opts;
    gen_opts.sampling.temperature = static_cast<float>(config.temperature);
    gen_opts.max_tokens = config.max_response_tokens;

    zoo::AgentConfig agent_config;
    agent_config.max_tool_iterations = config.max_tool_iterations;

    auto result = zoo::Agent::create(model_config, agent_config, gen_opts);
    if (!result) {
        logging::write(logging::Level::Error, "ai",
                       "zoo::Agent::create failed: " + result.error().to_string());
        throw std::runtime_error("NpcAgentPool::from_config: failed to create zoo::Agent: " +
                                 result.error().to_string());
    }

    logging::write(logging::Level::Info, "ai", "zoo agent created");
    return NpcAgentPool(
        std::make_unique<ZooAgentAdapter>(std::move(*result), config.inference_timeout_ms));
}

NpcAgentHandle NpcAgentPool::acquire(const std::string &npc_id) {
    if (in_use_) {
        logging::write(logging::Level::Error, "ai",
                       "agent acquire failed; already in use npc=" + npc_id);
        throw std::runtime_error("NpcAgentPool: agent is already in use");
    }
    in_use_ = true;
    logging::write(logging::Level::Debug, "ai", "acquiring shared agent npc=" + npc_id);
    agent_->clear_history();
    return NpcAgentHandle(agent_.get(), this, npc_id);
}

void NpcAgentPool::release() {
    logging::write(logging::Level::Debug, "ai", "releasing shared agent");
    agent_->clear_history();
    in_use_ = false;
}

} // namespace chronicle
