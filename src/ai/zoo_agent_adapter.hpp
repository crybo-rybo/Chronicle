#pragma once
#include "ai/agent_interface.hpp"
#include <memory>
#include <zoo/agent.hpp>

namespace chronicle {

/// Concrete AgentInterface implementation wrapping a real zoo::Agent.
class ZooAgentAdapter : public AgentInterface {
public:
    explicit ZooAgentAdapter(std::unique_ptr<zoo::Agent> agent);

    void set_system_prompt(std::string_view prompt) override;
    void clear_history() override;
    bool is_running() const noexcept override;

    /// Access the underlying zoo::Agent for chat/tool calls in Sprint 3+.
    zoo::Agent &agent() { return *agent_; }
    const zoo::Agent &agent() const { return *agent_; }

private:
    std::unique_ptr<zoo::Agent> agent_;
};

} // namespace chronicle
