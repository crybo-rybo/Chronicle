/**
 * @file zoo_agent_adapter.cpp
 * @brief Implementation of @ref ZooAgentAdapter — Zoo-Keeper agent wrapper.
 *
 * @details Adapts @c zoo::Agent calls to the @ref AgentInterface contract.
 * The polling loop in @ref ZooAgentAdapter::chat_streaming drives the
 * streaming token display by allowing the main thread to drain the
 * @ref TokenQueue between 10 ms sleep intervals.
 */

#include "ai/zoo_agent_adapter.hpp"
#include "ai/tool_registry.hpp"
#include <chrono>
#include <stdexcept>
#include <thread>

namespace chronicle {

ZooAgentAdapter::ZooAgentAdapter(std::unique_ptr<zoo::Agent> agent) : agent_(std::move(agent)) {
    if (!agent_) {
        throw std::invalid_argument("ZooAgentAdapter: agent must not be null");
    }
}

void ZooAgentAdapter::set_system_prompt(std::string_view prompt) {
    agent_->set_system_prompt(prompt);
}

void ZooAgentAdapter::clear_history() {
    agent_->clear_history();
}

bool ZooAgentAdapter::is_running() const noexcept {
    return agent_->is_running();
}

void ZooAgentAdapter::register_tools(ToolRegistry &tool_registry, const std::string &npc_id) {
    tool_registry.set_active_npc_id(npc_id);
    if (registered_tool_registry_ != &tool_registry) {
        tool_registry.register_tools(*agent_, npc_id);
        registered_tool_registry_ = &tool_registry;
    }
}

AgentChatResult ZooAgentAdapter::chat_streaming(std::string_view user_message,
                                                TokenCallback on_token,
                                                PollCallback poll) {
    auto handle = agent_->chat(user_message, {}, std::move(on_token));
    while (!handle.ready()) {
        if (poll) {
            poll();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (poll) {
        poll();
    }

    auto result = handle.await_result();
    if (!result) {
        return AgentChatResult{false, result.error().to_string()};
    }

    if (poll) {
        poll();
    }
    return AgentChatResult{true, ""};
}

} // namespace chronicle
