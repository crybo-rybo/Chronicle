/**
 * @file npc_agent_pool.cpp
 * @brief Implementation of @ref NpcAgentPool and @ref NpcAgentHandle.
 */

#include "ai/npc_agent_pool.hpp"
#include "ai/harness_compat.hpp"
#include "ai/tool_registry.hpp"
#include "diagnostics/logger.hpp"
#include "entities/config.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>

#if CHRONICLE_ENABLE_HARNESS
#include "ai/harness_agent_adapter.hpp"

#include <zoo/Agent.hpp>
#endif

namespace chronicle {

#if CHRONICLE_ENABLE_HARNESS
namespace {

void validate_harness_config(const Config &config) {
    if (!config.has_llm_endpoint()) {
        throw std::runtime_error(
            "NpcAgentPool::from_config: llm_base_url and llm_model are required for "
            "harness-backed dialogue.");
    }
    if (config.llm_http_timeout_ms <= 0) {
        throw std::runtime_error(
            "NpcAgentPool::from_config: llm_http_timeout_ms must be positive.");
    }
    if (config.llm_max_retries < 0) {
        throw std::runtime_error("NpcAgentPool::from_config: llm_max_retries cannot be negative.");
    }
    if (config.max_response_tokens <= 0) {
        throw std::runtime_error(
            "NpcAgentPool::from_config: max_response_tokens must be positive.");
    }
    if (config.max_tool_iterations <= 0) {
        throw std::runtime_error(
            "NpcAgentPool::from_config: max_tool_iterations must be positive.");
    }
    if (config.temperature < 0.0) {
        throw std::runtime_error("NpcAgentPool::from_config: temperature must be non-negative.");
    }
}

zoo::EndpointConfig build_endpoint_config(const Config &config) {
    zoo::EndpointConfig endpoint;
    endpoint.base_url = config.llm_base_url;
    endpoint.model = config.llm_model;
    endpoint.api_key = config.llm_api_key;
    endpoint.organization = config.llm_organization;
    endpoint.timeout = std::chrono::milliseconds(config.llm_http_timeout_ms);
    endpoint.max_retries = config.llm_max_retries;
    endpoint.tls_verify = config.llm_tls_verify;
    return endpoint;
}

zoo::GenerationOptions build_generation_options(const Config &config) {
    zoo::GenerationOptions options;
    options.sampling.temperature = static_cast<float>(config.temperature);
    options.max_tokens = config.max_response_tokens;
    return options;
}

} // namespace
#endif

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

NpcAgentPool NpcAgentPool::from_config(const Config &config, ToolRegistry &tool_registry) {
#if CHRONICLE_ENABLE_HARNESS
    validate_harness_config(config);

    logging::write(logging::Level::Info, "ai",
                   "creating harness agent base_url=" + config.llm_base_url +
                       " model=" + config.llm_model +
                       " max_response_tokens=" + std::to_string(config.max_response_tokens) +
                       " temperature=" + std::to_string(config.temperature) +
                       " max_tool_iterations=" + std::to_string(config.max_tool_iterations) +
                       " inference_timeout_ms=" + std::to_string(config.inference_timeout_ms) +
                       " llm_http_timeout_ms=" + std::to_string(config.llm_http_timeout_ms) +
                       " llm_max_retries=" + std::to_string(config.llm_max_retries) +
                       " llm_tls_verify=" + (config.llm_tls_verify ? "true" : "false"));

    zoo::RunConfig run_config;
    run_config.max_iterations = static_cast<size_t>(config.max_tool_iterations);
    run_config.default_options = build_generation_options(config);

    zoo::tools::ExecutionPolicy policy;
    policy.set_max_calls_per_run(static_cast<size_t>(config.max_tool_iterations));

    zoo::Agent::Builder builder;
    builder.endpoint(build_endpoint_config(config))
        .run_config(run_config)
        .policy(std::move(policy));
    tool_registry.register_harness_tools(builder);

    auto result = builder.build();
    if (!result) {
        logging::write(logging::Level::Error, "ai",
                       "zoo-keeper-harness Agent::build failed: " + result.error().to_string());
        throw std::runtime_error("NpcAgentPool::from_config: failed to create harness agent: " +
                                 result.error().to_string());
    }

    logging::write(logging::Level::Info, "ai", "harness agent created");
    auto agent = std::make_unique<zoo::Agent>(std::move(*result));
    return NpcAgentPool(std::make_unique<HarnessAgentAdapter>(std::move(agent), tool_registry,
                                                              config.inference_timeout_ms));
#else
    (void)config;
    (void)tool_registry;
    logging::write(logging::Level::Error, "ai",
                   "LLM endpoint was configured but Chronicle was built without harness support");
    throw_harness_disabled("NpcAgentPool::from_config");
#endif
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
