#pragma once
#include "ai/agent_interface.hpp"
#include "ai/tool_registry.hpp"
#include <chrono>
#include <stdexcept>
#include <thread>

namespace chronicle {

/// @brief Configurable mock agent for testing failure modes.
class FailureMockAgent : public AgentInterface {
  public:
    enum class Mode {
        ThrowOnChat,       ///< chat_streaming throws an exception.
        ReturnFailure,     ///< chat_streaming returns success=false.
        StreamEmpty,       ///< chat_streaming returns success=true but emits no tokens.
        MalformedToolArgs, ///< Drives a malformed string tool path without leaking mutations.
        HangBriefly,       ///< Polls while briefly blocking, then returns success.
    };

    Mode mode = Mode::StreamEmpty;

    void set_system_prompt(std::string_view) override {}
    void add_system_message(std::string_view) override {}
    void clear_history() override {}
    bool is_running() const noexcept override { return false; }
    void register_tools(ToolRegistry &tool_registry, const std::string &npc_id) override {
        tool_registry_ = &tool_registry;
        npc_id_ = npc_id;
        tool_registry_->set_active_npc_id(npc_id_);
    }

    AgentChatResult chat_streaming(std::string_view, TokenCallback, PollCallback poll) override {
        if (poll)
            poll();
        switch (mode) {
        case Mode::ThrowOnChat:
            throw std::runtime_error("Mock agent: simulated inference failure");
        case Mode::ReturnFailure:
            return AgentChatResult{false, "Mock agent: simulated chat failure"};
        case Mode::StreamEmpty:
            return AgentChatResult{true, ""};
        case Mode::MalformedToolArgs:
            if (tool_registry_) {
                (void)tool_registry_->handle_set_flag_tool("test_flag", "not-a-bool");
            }
            return AgentChatResult{true, ""};
        case Mode::HangBriefly:
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            if (poll)
                poll();
            return AgentChatResult{true, ""};
        }
        return AgentChatResult{true, ""};
    }

  private:
    ToolRegistry *tool_registry_ = nullptr;
    std::string npc_id_;
};

} // namespace chronicle
