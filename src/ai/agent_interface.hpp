#pragma once
#include <functional>
#include <string>
#include <string_view>

namespace chronicle {

class ToolRegistry;

struct AgentChatResult {
    bool success = false;
    std::string error_message;
};

/// Thin abstract base class extracting the zoo::Agent lifecycle methods Chronicle needs.
class AgentInterface {
public:
    using TokenCallback = std::function<void(std::string_view)>;
    using PollCallback = std::function<void()>;

    virtual ~AgentInterface() = default;
    virtual void set_system_prompt(std::string_view prompt) = 0;
    virtual void clear_history() = 0;
    virtual bool is_running() const noexcept = 0;
    virtual void register_tools(ToolRegistry &tool_registry, const std::string &npc_id) = 0;
    virtual AgentChatResult chat_streaming(std::string_view user_message,
                                           TokenCallback on_token,
                                           PollCallback poll) = 0;
};

} // namespace chronicle
