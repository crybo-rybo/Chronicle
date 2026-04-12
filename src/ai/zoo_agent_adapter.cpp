#include "ai/zoo_agent_adapter.hpp"
#include <stdexcept>

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

} // namespace chronicle
