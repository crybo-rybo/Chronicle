#pragma once
#include <string_view>

namespace chronicle {

/// Thin abstract base class extracting the zoo::Agent lifecycle methods Chronicle needs.
/// Sprint 2 skeleton — chat methods will be added in later sprints.
class AgentInterface {
public:
    virtual ~AgentInterface() = default;
    virtual void set_system_prompt(std::string_view prompt) = 0;
    virtual void clear_history() = 0;
    virtual bool is_running() const noexcept = 0;
};

} // namespace chronicle
