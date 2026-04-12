#include "ai/npc_agent_pool.hpp"
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// MockAgent — tracks lifecycle calls for testing
// ---------------------------------------------------------------------------

class MockAgent : public AgentInterface {
public:
    int clear_history_call_count = 0;
    std::string last_system_prompt;
    bool running = true;

    void set_system_prompt(std::string_view prompt) override {
        last_system_prompt = std::string(prompt);
    }

    void clear_history() override { ++clear_history_call_count; }

    bool is_running() const noexcept override { return running; }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(NpcAgentPoolTest, AcquireReturnsValidHandle) {
    auto mock = std::make_unique<MockAgent>();
    NpcAgentPool pool(std::move(mock));

    auto handle = pool.acquire("marcus");
    EXPECT_NE(handle.operator->(), nullptr);
}

TEST(NpcAgentPoolTest, HandleDestructorCallsClearHistory) {
    auto mock = std::make_unique<MockAgent>();
    auto* raw = mock.get();
    NpcAgentPool pool(std::move(mock));

    {
        auto handle = pool.acquire("marcus");
        // acquire calls clear_history once
        EXPECT_EQ(raw->clear_history_call_count, 1);
    }
    // destructor calls release, which calls clear_history again
    EXPECT_EQ(raw->clear_history_call_count, 2);
}

TEST(NpcAgentPoolTest, ReleaseAllowsReacquire) {
    auto mock = std::make_unique<MockAgent>();
    NpcAgentPool pool(std::move(mock));

    {
        auto handle = pool.acquire("marcus");
    }
    // Should not throw — agent was released
    EXPECT_NO_THROW({ auto handle = pool.acquire("elena"); });
}

TEST(NpcAgentPoolTest, DoubleAcquireThrows) {
    auto mock = std::make_unique<MockAgent>();
    NpcAgentPool pool(std::move(mock));

    auto handle = pool.acquire("marcus");
    EXPECT_THROW(pool.acquire("elena"), std::runtime_error);
}

TEST(NpcAgentPoolTest, HandleMoveSemantics) {
    auto mock = std::make_unique<MockAgent>();
    auto* raw = mock.get();
    NpcAgentPool pool(std::move(mock));

    int count_before;
    {
        auto handle1 = pool.acquire("marcus");
        count_before = raw->clear_history_call_count;

        auto handle2 = std::move(handle1);

        // Destroying the moved-from handle1 should NOT call release
    }
    // Only one release should have happened (from handle2's destructor)
    EXPECT_EQ(raw->clear_history_call_count, count_before + 1);
}

TEST(NpcAgentPoolTest, HandleNpcId) {
    auto mock = std::make_unique<MockAgent>();
    NpcAgentPool pool(std::move(mock));

    auto handle = pool.acquire("marcus");
    EXPECT_EQ(handle.npc_id(), "marcus");
}

} // namespace chronicle
