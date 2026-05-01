#include "ai/npc_agent_pool.hpp"
#include "engine/game_engine.hpp"
#include "mocks/mock_agent.hpp"
#include "rendering/renderer.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace chronicle {

namespace {

std::filesystem::path fixture_root() {
    return std::filesystem::path(FIXTURES_DIR);
}

WorldFileSet fixture_world_files() {
    auto root = fixture_root();
    return WorldFileSet{.world = root / "world.json",
                        .npcs = root / "npcs.json",
                        .facts = root / "facts.json",
                        .flags = root / "flags.json",
                        .events = root / "events.json"};
}

} // namespace

// ---------------------------------------------------------------------------
// Test renderer that captures errors
// ---------------------------------------------------------------------------

class ErrorCapturingRenderer : public Renderer {
  public:
    std::vector<std::string> errors;
    std::vector<std::string> systems;

    void render_scene(const Location &, const World &) override {}
    void render_move(const std::string &, const std::string &) override {}
    void render_npc_intro(std::string_view, std::string_view) override {}
    void begin_npc_dialogue(std::string_view) override {}
    void stream_token(std::string_view) override {}
    void flush_dialogue() override {}
    void render_action(std::string_view) override {}
    void render_inventory(const Player &, const World &) override {}
    void render_item_examine(const Item &) override {}
    void render_error(std::string_view msg) override { errors.emplace_back(msg); }
    void render_system(std::string_view msg) override { systems.emplace_back(msg); }
    void render_time_advance(const Clock &) override {}
    void render_resolution(std::string_view) override {}
    std::string get_player_input(std::string_view) override { return ""; }
    void clear_input_line() override {}
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class AgentFailureTest : public ::testing::Test {
  protected:
    std::unique_ptr<GameEngine> engine;
    ErrorCapturingRenderer *renderer_ptr = nullptr;

    void build_engine(FailureMockAgent::Mode mode) {
        auto mock = std::make_unique<FailureMockAgent>();
        mock->mode = mode;

        auto pool = std::make_unique<NpcAgentPool>(std::move(mock));
        auto renderer = std::make_unique<ErrorCapturingRenderer>();
        renderer_ptr = renderer.get();

        engine = std::make_unique<GameEngine>((fixture_root() / "config.json").string(),
                                              fixture_world_files(), std::move(renderer),
                                              std::move(pool));
    }

    void start_conversation() {
        ParsedCommand talk;
        talk.verb = CommandVerb::Talk;
        talk.primary_arg = "test_npc";
        talk.raw_input = "talk test_npc";
        engine->handle_command(talk);
    }

    void send_dialogue(const std::string &text) {
        ParsedCommand dialogue;
        dialogue.verb = CommandVerb::Dialogue;
        dialogue.raw_input = text;
        dialogue.primary_arg = text;
        engine->handle_command(dialogue);
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(AgentFailureTest, ThrowOnChatKeepsGameAlive) {
    build_engine(FailureMockAgent::Mode::ThrowOnChat);
    start_conversation();
    ASSERT_EQ(engine->phase(), GamePhase::InConversation);

    EXPECT_NO_THROW(send_dialogue("Hello"));

    // Error was rendered
    EXPECT_FALSE(renderer_ptr->errors.empty());
    // World state unchanged — no pending mutations leaked
    EXPECT_TRUE(engine->world().player.inventory.empty() ||
                engine->world().player.inventory == std::vector<std::string>{});
}

TEST_F(AgentFailureTest, ReturnFailureRendersError) {
    build_engine(FailureMockAgent::Mode::ReturnFailure);
    start_conversation();
    ASSERT_EQ(engine->phase(), GamePhase::InConversation);

    EXPECT_NO_THROW(send_dialogue("Hello"));

    bool has_error = false;
    for (const auto &e : renderer_ptr->errors) {
        if (e.find("failed") != std::string::npos || e.find("Failed") != std::string::npos) {
            has_error = true;
            break;
        }
    }
    EXPECT_TRUE(has_error);
}

TEST_F(AgentFailureTest, StreamEmptyNoMutationsLeaked) {
    build_engine(FailureMockAgent::Mode::StreamEmpty);
    start_conversation();
    ASSERT_EQ(engine->phase(), GamePhase::InConversation);

    EXPECT_NO_THROW(send_dialogue("Hello"));

    // Game loop stays alive
    EXPECT_EQ(engine->phase(), GamePhase::InConversation);
}

TEST_F(AgentFailureTest, MalformedToolArgsNoMutationsLeaked) {
    build_engine(FailureMockAgent::Mode::MalformedToolArgs);
    start_conversation();
    ASSERT_EQ(engine->phase(), GamePhase::InConversation);
    ASSERT_FALSE(engine->world().flags.at("test_flag"));

    EXPECT_NO_THROW(send_dialogue("Hello"));

    EXPECT_EQ(engine->phase(), GamePhase::InConversation);
    EXPECT_FALSE(engine->world().flags.at("test_flag"));
}

TEST_F(AgentFailureTest, HangBrieflyKeepsGameAlive) {
    build_engine(FailureMockAgent::Mode::HangBriefly);
    start_conversation();
    ASSERT_EQ(engine->phase(), GamePhase::InConversation);

    EXPECT_NO_THROW(send_dialogue("Hello"));

    EXPECT_EQ(engine->phase(), GamePhase::InConversation);
}

} // namespace chronicle
