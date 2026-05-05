#include "ai/npc_agent_pool.hpp"
#include "engine/game_engine.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <ranges>

using namespace chronicle;

class MockEngineRenderer : public Renderer {
  public:
    std::vector<std::string> actions;
    std::vector<std::string> systems;
    std::vector<std::string> errors;
    std::vector<std::string> tokens;
    std::vector<std::string> examined_items;
    int render_scene_count = 0;
    int time_advance_count = 0;
    Clock last_time_advance;
    std::string last_move_dir;
    std::string last_move_loc;

    void render_scene(const Location &, const World &) override { ++render_scene_count; }
    void render_move(const std::string &direction, const std::string &new_loc) override {
        last_move_dir = direction;
        last_move_loc = new_loc;
    }
    void render_npc_intro(std::string_view, std::string_view) override {}
    void begin_npc_dialogue(std::string_view) override {}
    void stream_token(std::string_view token) override { tokens.push_back(std::string(token)); }
    void flush_dialogue() override {}
    void render_action(std::string_view narration) override {
        actions.push_back(std::string(narration));
    }
    void render_inventory(const Player &, const World &) override {}
    void render_item_examine(const Item &item) override { examined_items.push_back(item.id); }
    void render_error(std::string_view message) override { errors.push_back(std::string(message)); }
    void render_system(std::string_view message) override {
        systems.push_back(std::string(message));
    }
    void render_time_advance(const Clock &clock) override {
        ++time_advance_count;
        last_time_advance = clock;
    }
    void render_resolution(std::string_view) override {}
    std::string get_player_input(std::string_view) override { return ""; }
    void clear_input_line() override {}
};

class FakeDialogueAgent : public AgentInterface {
  public:
    ToolRegistry *registry = nullptr;
    std::string active_npc_id;
    std::string last_system_prompt;
    std::vector<std::string> system_messages;
    std::string last_user_message;
    std::vector<std::string> user_messages;
    int clear_history_call_count = 0;
    int register_tools_call_count = 0;
    int set_system_prompt_call_count = 0;

    void set_system_prompt(std::string_view prompt) override {
        last_system_prompt = std::string(prompt);
        ++set_system_prompt_call_count;
    }

    void add_system_message(std::string_view message) override {
        system_messages.emplace_back(message);
    }

    void clear_history() override { ++clear_history_call_count; }

    bool is_running() const noexcept override { return false; }

    void register_tools(ToolRegistry &tool_registry, const std::string &npc_id) override {
        registry = &tool_registry;
        active_npc_id = npc_id;
        ++register_tools_call_count;
    }

    AgentChatResult chat_streaming(std::string_view user_message,
                                   AgentInterface::TokenCallback on_token,
                                   AgentInterface::PollCallback poll) override {
        last_user_message = std::string(user_message);
        user_messages.push_back(last_user_message);
        on_token("Take ");
        poll();
        on_token("this.");
        poll();
        registry->register_give_item(active_npc_id, "cargo_manifest");
        return AgentChatResult{true, ""};
    }
};

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

std::filesystem::path sample_root() {
    return std::filesystem::path(CHRONICLE_SOURCE_DIR) / "data";
}

WorldFileSet sample_world_files() {
    auto root = sample_root();
    return WorldFileSet{.world = root / "world.json",
                        .npcs = root / "npcs.json",
                        .facts = root / "facts.json",
                        .flags = root / "flags.json",
                        .events = root / "events.json"};
}

} // namespace

class GameEngineTest : public ::testing::Test {
  protected:
    std::unique_ptr<MockEngineRenderer> mock_renderer;

    void SetUp() override { mock_renderer = std::make_unique<MockEngineRenderer>(); }
};

TEST_F(GameEngineTest, HandlesGoCommandValid) {
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(mock_renderer));

    ParsedCommand cmd;
    cmd.verb = CommandVerb::Go;
    cmd.primary_arg = "north";
    engine.handle_command(cmd);

    EXPECT_EQ(engine.phase(), GamePhase::Playing);
    EXPECT_EQ(engine.world().player.current_location, "market_square");
    EXPECT_EQ(engine.world().clock.total_turns, 1);
    EXPECT_EQ(engine.world().total_turns_elapsed, 1);
}

TEST_F(GameEngineTest, GoInvalidExitRendersError) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(mock_renderer));

    ParsedCommand cmd;
    cmd.verb = CommandVerb::Go;
    cmd.primary_arg = "invalid_dir";
    engine.handle_command(cmd);

    ASSERT_EQ(renderer->errors.size(), 1u);
    EXPECT_EQ(renderer->errors[0], "You can't go that way.");
}

TEST_F(GameEngineTest, TakeAddsItemToInventoryAndRemovesFromLocation) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(mock_renderer));

    ParsedCommand cmd;
    cmd.verb = CommandVerb::Take;
    cmd.primary_arg = "test item";
    engine.handle_command(cmd);

    EXPECT_TRUE(std::ranges::contains(engine.world().player.inventory, "test_item"));
    EXPECT_TRUE(engine.world().locations.at("test_room").items.empty());
    EXPECT_EQ(renderer->actions[0], "You take the Test Item.");
}

TEST_F(GameEngineTest, DropMovesItemFromInventoryBackToLocation) {
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(mock_renderer));

    ParsedCommand take;
    take.verb = CommandVerb::Take;
    take.primary_arg = "test item";
    engine.handle_command(take);

    ParsedCommand drop;
    drop.verb = CommandVerb::Drop;
    drop.primary_arg = "test item";
    engine.handle_command(drop);

    EXPECT_FALSE(std::ranges::contains(engine.world().player.inventory, "test_item"));
    EXPECT_TRUE(std::ranges::contains(engine.world().locations.at("test_room").items, "test_item"));
    EXPECT_EQ(engine.world().clock.total_turns, 2);
    EXPECT_EQ(engine.world().total_turns_elapsed, 2);
}

TEST_F(GameEngineTest, ExamineVisibleLocationItemRendersItem) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(mock_renderer));

    ParsedCommand cmd;
    cmd.verb = CommandVerb::Examine;
    cmd.primary_arg = "test item";
    engine.handle_command(cmd);

    ASSERT_EQ(renderer->examined_items.size(), 1u);
    EXPECT_EQ(renderer->examined_items[0], "test_item");
}

TEST_F(GameEngineTest, SaveAndLoadDefaultSlotRoundtripsWorld) {
    auto save_dir = std::filesystem::path("/tmp/chronicle_saves");
    std::filesystem::remove(save_dir / "slot_1.json");

    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(mock_renderer));

    ParsedCommand take;
    take.verb = CommandVerb::Take;
    take.primary_arg = "test item";
    engine.handle_command(take);

    ParsedCommand save;
    save.verb = CommandVerb::Save;
    engine.handle_command(save);

    ParsedCommand drop;
    drop.verb = CommandVerb::Drop;
    drop.primary_arg = "test item";
    engine.handle_command(drop);

    ParsedCommand load;
    load.verb = CommandVerb::Load;
    engine.handle_command(load);

    EXPECT_TRUE(std::ranges::contains(engine.world().player.inventory, "test_item"));
    EXPECT_EQ(engine.world().clock.total_turns, 1);
    std::filesystem::remove(save_dir / "slot_1.json");
}

TEST_F(GameEngineTest, DialogueUsesCurrentConversationNpcAndStreamsTokens) {
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    auto fake_agent = std::make_unique<FakeDialogueAgent>();
    auto *fake_agent_ptr = fake_agent.get();
    auto pool = std::make_unique<NpcAgentPool>(std::move(fake_agent));
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(renderer), std::move(pool));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "Can I see the manifest?";
    dialogue.primary_arg = "Can I see the manifest?";
    engine.handle_command(dialogue);

    EXPECT_EQ(fake_agent_ptr->active_npc_id, "marcus");
    EXPECT_EQ(renderer_ptr->tokens, (std::vector<std::string>{"Take ", "this."}));
    EXPECT_TRUE(std::ranges::contains(engine.world().player.inventory, "cargo_manifest"));
    EXPECT_FALSE(
        std::ranges::contains(engine.world().npcs.at("marcus").state.inventory, "cargo_manifest"));
    EXPECT_EQ(engine.world().clock.total_turns, 1);
    ASSERT_FALSE(renderer_ptr->actions.empty());
    EXPECT_EQ(renderer_ptr->actions.back(), "Marcus hands you the Cargo Manifest.");
}

TEST_F(GameEngineTest, DialogueInjectsDynamicContextAsSystemMessageAndKeepsUserTurnClean) {
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto fake_agent = std::make_unique<FakeDialogueAgent>();
    auto *fake_agent_ptr = fake_agent.get();
    auto pool = std::make_unique<NpcAgentPool>(std::move(fake_agent));
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(renderer), std::move(pool));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "Can I see the manifest?";
    dialogue.primary_arg = "Can I see the manifest?";
    engine.handle_command(dialogue);

    ASSERT_EQ(fake_agent_ptr->system_messages.size(), 1u);
    EXPECT_NE(fake_agent_ptr->system_messages.back().find("[Current state]"), std::string::npos);
    EXPECT_NE(fake_agent_ptr->system_messages.back().find("Current mood:"), std::string::npos);
    EXPECT_NE(fake_agent_ptr->system_messages.back().find("Trust toward the player:"),
              std::string::npos);

    EXPECT_EQ(fake_agent_ptr->last_user_message.find("[Current state]"), std::string::npos);
    EXPECT_NE(
        fake_agent_ptr->last_user_message.find("The player says: \"Can I see the manifest?\""),
        std::string::npos);
}

TEST_F(GameEngineTest, DialogueExitClearsConversation) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(mock_renderer));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);
    ASSERT_EQ(engine.phase(), GamePhase::InConversation);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "bye";
    dialogue.primary_arg = "bye";
    engine.handle_command(dialogue);

    EXPECT_EQ(engine.phase(), GamePhase::Playing);
    EXPECT_FALSE(renderer->systems.empty());
}

TEST_F(GameEngineTest, HandlesQuitCommand) {
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(mock_renderer));

    ParsedCommand cmd;
    cmd.verb = CommandVerb::Quit;
    engine.handle_command(cmd);

    // The running flag becomes false.
}

TEST_F(GameEngineTest, TalkCommandInitializesAgent) {
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto fake_agent = std::make_unique<FakeDialogueAgent>();
    auto *fake_agent_ptr = fake_agent.get();
    auto pool = std::make_unique<NpcAgentPool>(std::move(fake_agent));
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(renderer), std::move(pool));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);

    EXPECT_EQ(engine.phase(), GamePhase::InConversation);
    EXPECT_EQ(fake_agent_ptr->set_system_prompt_call_count, 1);
    EXPECT_EQ(fake_agent_ptr->register_tools_call_count, 1);
}

TEST_F(GameEngineTest, MultiTurnDialogueDoesNotReacquireAgent) {
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto fake_agent = std::make_unique<FakeDialogueAgent>();
    auto *fake_agent_ptr = fake_agent.get();
    auto pool = std::make_unique<NpcAgentPool>(std::move(fake_agent));
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(renderer), std::move(pool));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);

    ParsedCommand d1;
    d1.verb = CommandVerb::Dialogue;
    d1.raw_input = "Hello Marcus";
    d1.primary_arg = "Hello Marcus";
    engine.handle_command(d1);

    ParsedCommand d2;
    d2.verb = CommandVerb::Dialogue;
    d2.raw_input = "Tell me more";
    d2.primary_arg = "Tell me more";
    engine.handle_command(d2);

    EXPECT_EQ(fake_agent_ptr->set_system_prompt_call_count, 1);
    EXPECT_EQ(fake_agent_ptr->register_tools_call_count, 1);
    EXPECT_EQ(fake_agent_ptr->clear_history_call_count, 1);
    EXPECT_EQ(fake_agent_ptr->system_messages.size(), 2u);
    EXPECT_EQ(engine.world().clock.total_turns, 2);
    ASSERT_EQ(fake_agent_ptr->user_messages.size(), 2u);
    EXPECT_EQ(fake_agent_ptr->user_messages[0].find("[Current state]"), std::string::npos);
    EXPECT_EQ(fake_agent_ptr->user_messages[1].find("[Current state]"), std::string::npos);
}

TEST_F(GameEngineTest, LeaveConversationReleasesAgent) {
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto fake_agent = std::make_unique<FakeDialogueAgent>();
    auto *fake_agent_ptr = fake_agent.get();
    auto pool = std::make_unique<NpcAgentPool>(std::move(fake_agent));
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(renderer), std::move(pool));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);

    ParsedCommand bye;
    bye.verb = CommandVerb::Dialogue;
    bye.raw_input = "bye";
    bye.primary_arg = "bye";
    engine.handle_command(bye);

    EXPECT_EQ(fake_agent_ptr->clear_history_call_count, 2);
    EXPECT_EQ(engine.phase(), GamePhase::Playing);
}

TEST_F(GameEngineTest, NonAdvancingCommandsDoNotChangeClock) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(mock_renderer));

    ParsedCommand look;
    look.verb = CommandVerb::Look;
    engine.handle_command(look);

    ParsedCommand inventory;
    inventory.verb = CommandVerb::Inventory;
    engine.handle_command(inventory);

    ParsedCommand examine;
    examine.verb = CommandVerb::Examine;
    examine.primary_arg = "test item";
    engine.handle_command(examine);

    ParsedCommand failed_take;
    failed_take.verb = CommandVerb::Take;
    failed_take.primary_arg = "missing";
    engine.handle_command(failed_take);

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "test_npc";
    engine.handle_command(talk);

    ParsedCommand leave;
    leave.verb = CommandVerb::Dialogue;
    leave.raw_input = "bye";
    engine.handle_command(leave);

    EXPECT_EQ(engine.world().clock.total_turns, 0);
    EXPECT_EQ(engine.world().total_turns_elapsed, 0);
    EXPECT_EQ(renderer->time_advance_count, 0);
}

TEST_F(GameEngineTest, PeriodTransitionRendersOnceWhenThresholdCrosses) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(mock_renderer));

    const std::vector<std::string> directions = {"north", "south", "north", "south", "north"};
    for (const auto &direction : directions) {
        ParsedCommand go;
        go.verb = CommandVerb::Go;
        go.primary_arg = direction;
        engine.handle_command(go);
    }

    EXPECT_EQ(engine.world().clock.total_turns, 5);
    EXPECT_EQ(engine.world().clock.turns_this_period, 0);
    EXPECT_EQ(engine.world().clock.period, TimePeriod::Afternoon);
    EXPECT_EQ(renderer->time_advance_count, 1);
    EXPECT_EQ(renderer->last_time_advance.period, TimePeriod::Afternoon);
}
