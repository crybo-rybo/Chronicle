/**
 * @file harness_agent_adapter.cpp
 * @brief Implementation of @ref HarnessAgentAdapter.
 */

#include "ai/harness_agent_adapter.hpp"
#include "ai/tool_registry.hpp"
#include "diagnostics/logger.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace chronicle {

namespace {

std::string timeout_error_message(int timeout_ms) {
    return "Inference timed out after " + std::to_string(timeout_ms) + " ms.";
}

const char *stop_reason_name(zoo::RunStopReason reason) {
    switch (reason) {
    case zoo::RunStopReason::Completed:
        return "completed";
    case zoo::RunStopReason::MaxIterations:
        return "max_iterations";
    case zoo::RunStopReason::RunBudgetExceeded:
        return "run_budget_exceeded";
    }
    return "unknown";
}

class RunningScope {
  public:
    explicit RunningScope(bool &running) : running_(running) { running_ = true; }

    ~RunningScope() { running_ = false; }

    RunningScope(const RunningScope &) = delete;
    RunningScope &operator=(const RunningScope &) = delete;

  private:
    bool &running_;
};

} // namespace

HarnessAgentAdapter::HarnessAgentAdapter(std::unique_ptr<zoo::Agent> agent,
                                         ToolRegistry &tool_registry, int inference_timeout_ms)
    : agent_(std::move(agent)), tool_registry_(&tool_registry),
      inference_timeout_ms_(std::max(0, inference_timeout_ms)) {
    if (!agent_) {
        throw std::invalid_argument("HarnessAgentAdapter: agent must not be null");
    }
}

void HarnessAgentAdapter::apply_combined_system_prompt() {
    std::string prompt = static_system_prompt_;
    if (!dynamic_system_context_.empty()) {
        if (!prompt.empty()) {
            prompt += "\n\n";
        }
        prompt += dynamic_system_context_;
    }
    agent_->set_system_prompt(prompt);
}

void HarnessAgentAdapter::set_system_prompt(std::string_view prompt) {
    logging::write(logging::Level::Debug, "ai",
                   "setting static system prompt chars=" + std::to_string(prompt.size()));
    static_system_prompt_ = std::string(prompt);
    apply_combined_system_prompt();
}

void HarnessAgentAdapter::add_system_message(std::string_view message) {
    logging::write(logging::Level::Debug, "ai",
                   "refreshing dynamic system context chars=" + std::to_string(message.size()));
    dynamic_system_context_ = std::string(message);
    apply_combined_system_prompt();
}

void HarnessAgentAdapter::clear_history() {
    logging::write(logging::Level::Debug, "ai", "clearing harness agent history");
    agent_->clear_history();
    static_system_prompt_.clear();
    dynamic_system_context_.clear();
}

bool HarnessAgentAdapter::is_running() const noexcept {
    return running_;
}

void HarnessAgentAdapter::register_tools(ToolRegistry &tool_registry, const std::string &npc_id) {
    if (&tool_registry != tool_registry_) {
        throw std::invalid_argument(
            "HarnessAgentAdapter::register_tools called with a different ToolRegistry");
    }
    logging::write(logging::Level::Debug, "tools", "activating harness tools npc=" + npc_id);
    tool_registry_->set_active_npc_id(npc_id);
}

AgentChatResult HarnessAgentAdapter::chat_streaming(std::string_view user_message,
                                                    TokenCallback on_token, PollCallback poll) {
    logging::write(logging::Level::Info, "ai",
                   "starting harness run user_message_chars=" +
                       std::to_string(user_message.size()));

    const bool timeout_enabled = inference_timeout_ms_ > 0;
    const auto started_at = std::chrono::steady_clock::now();

    auto token_callback = [&](std::string_view token) -> zoo::TokenAction {
        if (on_token) {
            on_token(token);
        }
        if (poll) {
            poll();
        }
        if (timeout_enabled && std::chrono::steady_clock::now() - started_at >=
                                   std::chrono::milliseconds(inference_timeout_ms_)) {
            logging::write(logging::Level::Warning, "ai",
                           "harness run timed out during token streaming");
            return zoo::TokenAction::Stop;
        }
        return zoo::TokenAction::Continue;
    };

    auto cancel_callback = [&]() -> bool {
        if (poll) {
            poll();
        }
        return timeout_enabled && std::chrono::steady_clock::now() - started_at >=
                                      std::chrono::milliseconds(inference_timeout_ms_);
    };

    RunningScope running_scope(running_);
    auto result = agent_->run(user_message, {}, token_callback, cancel_callback);

    if (poll) {
        poll();
    }

    if (!result) {
        if (result.error().code == zoo::ErrorCode::RequestCancelled && timeout_enabled) {
            logging::write(logging::Level::Warning, "ai",
                           "harness run cancelled after timeout_ms=" +
                               std::to_string(inference_timeout_ms_));
            return AgentChatResult{false, timeout_error_message(inference_timeout_ms_)};
        }
        logging::write(logging::Level::Warning, "ai",
                       "harness run failed: " + result.error().to_string());
        return AgentChatResult{false, result.error().to_string()};
    }

    logging::write(
        logging::Level::Info, "ai",
        "harness run completed stop_reason=" + std::string(stop_reason_name(result->stop_reason)) +
            " tool_steps=" + std::to_string(result->steps.size()));

    return AgentChatResult{true, ""};
}

} // namespace chronicle
