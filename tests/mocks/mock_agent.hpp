#pragma once
#include "ai/agent_interface.hpp"
#include <stdexcept>

namespace chronicle {

/// @brief Configurable mock agent for testing failure modes.
class FailureMockAgent : public AgentInterface {
public:
    enum class Mode {
        ThrowOnChat,     ///< chat_streaming throws an exception.
        ReturnFailure,   ///< chat_streaming returns success=false.
        StreamEmpty,     ///< chat_streaming returns success=true but emits no tokens.
        MalformedToolArgs, ///< chat_streaming returns success=true (simulates bad tool calls).
    };

    Mode mode = Mode::StreamEmpty;

    void set_system_prompt(std::string_view) override {}
    void clear_history() override {}
    bool is_running() const noexcept override { return false; }
    void register_tools(ToolRegistry &, const std::string &) override {}

    AgentChatResult chat_streaming(std::string_view,
                                   TokenCallback,
                                   PollCallback poll) override {
        if (poll) poll();
        switch (mode) {
        case Mode::ThrowOnChat:
            throw std::runtime_error("Mock agent: simulated inference failure");
        case Mode::ReturnFailure:
            return AgentChatResult{false, "Mock agent: simulated chat failure"};
        case Mode::StreamEmpty:
            return AgentChatResult{true, ""};
        case Mode::MalformedToolArgs:
            return AgentChatResult{true, ""};
        }
        return AgentChatResult{true, ""};
    }
};

} // namespace chronicle
