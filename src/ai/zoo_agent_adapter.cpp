#include "ai/zoo_agent_adapter.hpp"
#include "ai/tool_registry.hpp"
#include <chrono>
#include <iostream>
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
    std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::register_tools npc_id="
              << npc_id << std::endl;
    tool_registry.set_active_npc_id(npc_id);
    if (registered_tool_registry_ != &tool_registry) {
        std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::register_tools registering on zoo::Agent"
                  << std::endl;
        tool_registry.register_tools(*agent_, npc_id);
        registered_tool_registry_ = &tool_registry;
    } else {
        std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::register_tools reused existing zoo tool registrations"
                  << std::endl;
    }
}

AgentChatResult ZooAgentAdapter::chat_streaming(std::string_view user_message,
                                                TokenCallback on_token,
                                                PollCallback poll) {
    auto start = std::chrono::steady_clock::now();
    std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::chat_streaming before agent_->chat, user_message_size="
              << user_message.size() << std::endl;
    auto handle = agent_->chat(user_message, {}, std::move(on_token));
    std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::chat_streaming queued request id="
              << handle.id() << std::endl;
    auto last_log = start;
    while (!handle.ready()) {
        if (poll) {
            poll();
        }
        auto now = std::chrono::steady_clock::now();
        if (now - last_log >= std::chrono::seconds(5)) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::chat_streaming still waiting after "
                      << elapsed.count() << "ms" << std::endl;
            last_log = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (poll) {
        poll();
    }

    std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::chat_streaming handle ready; awaiting result"
              << std::endl;
    auto result = handle.await_result();
    if (!result) {
        std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::chat_streaming await_result failed: "
                  << result.error().to_string() << std::endl;
        return AgentChatResult{false, result.error().to_string()};
    }

    if (poll) {
        poll();
    }
    std::cerr << "[ChronicleIntegrationDebug] ZooAgentAdapter::chat_streaming success, prompt_tokens="
              << result->usage.prompt_tokens << ", completion_tokens="
              << result->usage.completion_tokens << ", latency_ms="
              << result->metrics.latency_ms.count() << std::endl;
    return AgentChatResult{true, ""};
}

} // namespace chronicle
