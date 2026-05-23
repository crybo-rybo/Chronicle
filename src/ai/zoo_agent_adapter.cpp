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
#include "diagnostics/logger.hpp"
#include <algorithm>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <thread>

namespace chronicle {

namespace {

constexpr auto kPollInterval = std::chrono::milliseconds(10);
constexpr auto kCancelDrainTimeout = std::chrono::milliseconds(250);

std::string timeout_error_message(int timeout_ms) {
    return "Inference timed out after " + std::to_string(timeout_ms) + " ms.";
}

template <typename Handle>
std::optional<AgentChatResult> poll_until_ready_or_timeout(Handle &handle,
                                                           const AgentInterface::PollCallback &poll,
                                                           int timeout_ms, std::string_view phase) {
    const bool timeout_enabled = timeout_ms > 0;
    const auto started_at = std::chrono::steady_clock::now();

    while (!handle.ready()) {
        if (poll) {
            poll();
        }
        if (handle.ready()) {
            break;
        }
        if (timeout_enabled && std::chrono::steady_clock::now() - started_at >=
                                   std::chrono::milliseconds(timeout_ms)) {
            logging::write(logging::Level::Warning, "ai",
                           std::string(phase) + " timed out; requesting Zoo-Keeper cancellation");
            handle.cancel();

            const auto cancel_started_at = std::chrono::steady_clock::now();
            while (!handle.ready() &&
                   std::chrono::steady_clock::now() - cancel_started_at < kCancelDrainTimeout) {
                if (poll) {
                    poll();
                }
                std::this_thread::sleep_for(kPollInterval);
            }

            if (handle.ready()) {
                auto cancelled_result = handle.await_result();
                if (!cancelled_result) {
                    logging::write(logging::Level::Info, "ai",
                                   "cancelled " + std::string(phase) +
                                       " result: " + cancelled_result.error().to_string());
                }
            }
            if (poll) {
                poll();
            }
            return AgentChatResult{false, timeout_error_message(timeout_ms)};
        }
        std::this_thread::sleep_for(kPollInterval);
    }

    return std::nullopt;
}

} // namespace

ZooAgentAdapter::ZooAgentAdapter(std::unique_ptr<zoo::Agent> agent, int inference_timeout_ms)
    : agent_(std::move(agent)), inference_timeout_ms_(std::max(0, inference_timeout_ms)) {
    if (!agent_) {
        throw std::invalid_argument("ZooAgentAdapter: agent must not be null");
    }
}

void ZooAgentAdapter::set_system_prompt(std::string_view prompt) {
    logging::write(logging::Level::Debug, "ai",
                   "setting system prompt chars=" + std::to_string(prompt.size()));
    agent_->set_system_prompt(prompt);
}

void ZooAgentAdapter::add_system_message(std::string_view message) {
    logging::write(logging::Level::Debug, "ai",
                   "injecting system message chars=" + std::to_string(message.size()));
    (void)agent_->add_system_message(message);
}

void ZooAgentAdapter::clear_history() {
    logging::write(logging::Level::Debug, "ai", "clearing zoo agent history");
    agent_->clear_history();
}

bool ZooAgentAdapter::is_running() const noexcept {
    return agent_->is_running();
}

void ZooAgentAdapter::register_tools(ToolRegistry &tool_registry, const std::string &npc_id) {
    tool_registry.set_active_npc_id(npc_id);
    if (registered_tool_registry_ != &tool_registry) {
        logging::write(logging::Level::Info, "ai",
                       "registering Chronicle tools on zoo agent npc=" + npc_id);
        tool_registry.register_zoo_tools(*agent_, npc_id);
        registered_tool_registry_ = &tool_registry;
    } else {
        logging::write(logging::Level::Debug, "ai",
                       "reusing existing tool registration npc=" + npc_id);
    }
}

AgentChatResult ZooAgentAdapter::chat_streaming(std::string_view user_message,
                                                TokenCallback on_token, PollCallback poll) {
    // Pass on_token as an lvalue (copy) so it remains usable for the nudge call below
    // if the tool loop limit is hit. zoo::AsyncTextCallback is std::function — copyable.
    logging::write(logging::Level::Info, "ai",
                   "starting zoo chat user_message_chars=" + std::to_string(user_message.size()));
    auto handle = agent_->chat(user_message, {}, on_token);
    if (auto timeout = poll_until_ready_or_timeout(handle, poll, inference_timeout_ms_, "chat")) {
        return *timeout;
    }

    if (poll) {
        poll();
    }

    auto result = handle.await_result();

    if (!result) {
        logging::write(logging::Level::Warning, "ai",
                       "zoo chat failed: " + result.error().to_string());
        // When the tool iteration budget is exhausted the NPC has produced no visible
        // response. Rather than surfacing the raw error to the player, issue a synthetic
        // nudge that asks the NPC to respond naturally without further tool calls.
        // The nudge tokens stream to the player via the same on_token callback.
        if (result.error().code == zoo::ErrorCode::ToolLoopLimitReached) {
            static constexpr std::string_view kNudge =
                "Please give a brief, natural response to continue the conversation. "
                "Do not call any tools.";

            logging::write(logging::Level::Warning, "ai",
                           "tool loop limit reached; issuing no-tool nudge");
            auto nudge_handle = agent_->chat(kNudge, {}, on_token);
            if (auto timeout = poll_until_ready_or_timeout(
                    nudge_handle, poll, inference_timeout_ms_, "tool-loop nudge")) {
                return *timeout;
            }

            if (poll) {
                poll();
            }

            auto nudge_result = nudge_handle.await_result();

            if (!nudge_result) {
                logging::write(logging::Level::Warning, "ai",
                               "zoo nudge failed: " + nudge_result.error().to_string());
                // If the nudge itself exhausted the tool budget, swallow the 504 —
                // the player should never see a raw error code regardless of which
                // call hit the limit.  Genuine errors (non-ToolLoopLimitReached)
                // still propagate so real failures aren't silently hidden.
                if (nudge_result.error().code == zoo::ErrorCode::ToolLoopLimitReached) {
                    if (poll) {
                        poll();
                    }
                    logging::write(logging::Level::Info, "ai",
                                   "swallowed repeated tool loop limit after nudge");
                    return AgentChatResult{true, ""};
                }
                return AgentChatResult{false, nudge_result.error().to_string()};
            }

            if (poll) {
                poll();
            }
            logging::write(logging::Level::Info, "ai", "zoo nudge completed");
            return AgentChatResult{true, ""};
        }

        return AgentChatResult{false, result.error().to_string()};
    }

    if (poll) {
        poll();
    }
    logging::write(logging::Level::Info, "ai", "zoo chat completed");
    return AgentChatResult{true, ""};
}

} // namespace chronicle
