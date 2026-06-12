#include "ai/npc_agent_pool.hpp"
#include "ai/tool_registry.hpp"
#include "entities/config.hpp"
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

    void add_system_message(std::string_view) override {}

    void clear_history() override { ++clear_history_call_count; }

    bool is_running() const noexcept override { return running; }

    void register_tools(ToolRegistry &, const std::string &) override {}

    AgentChatResult chat_streaming(std::string_view, AgentInterface::TokenCallback,
                                   AgentInterface::PollCallback) override {
        return AgentChatResult{true, ""};
    }
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
    auto *raw = mock.get();
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
    auto *raw = mock.get();
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

#if CHRONICLE_ENABLE_HARNESS
TEST(NpcAgentPoolTest, FromConfigThrowsOnMissingEndpointConfig) {
    Config config;
    World world;
    ToolRegistry registry(world);

    EXPECT_THROW(NpcAgentPool::from_config(config, registry), std::runtime_error);
}

TEST(NpcAgentPoolTest, FromConfigEndpointErrorMessageIsDescriptive) {
    Config config;
    World world;
    ToolRegistry registry(world);

    try {
        NpcAgentPool::from_config(config, registry);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error &e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("llm_base_url"), std::string::npos);
        EXPECT_NE(msg.find("llm_model"), std::string::npos);
    }
}

TEST(NpcAgentPoolTest, FromConfigBuildsHarnessAgentWithoutNetworkRequest) {
    Config config;
    config.llm_base_url = "http://localhost:11434/v1";
    config.llm_model = "test-model";
    World world;
    ToolRegistry registry(world);

    EXPECT_NO_THROW({ auto pool = NpcAgentPool::from_config(config, registry); });
}

TEST(NpcAgentPoolTest, FromConfigRejectsInvalidHarnessRuntimeLimits) {
    Config config;
    config.llm_base_url = "http://localhost:11434/v1";
    config.llm_model = "test-model";
    config.llm_http_timeout_ms = 0;
    World world;
    ToolRegistry registry(world);

    EXPECT_THROW(NpcAgentPool::from_config(config, registry), std::runtime_error);
}
#endif

} // namespace chronicle
