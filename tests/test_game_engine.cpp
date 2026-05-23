#include "ai/npc_agent_pool.hpp"
#include "engine/game_engine.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <nlohmann/json.hpp>
#include <ranges>
#include <stdexcept>

using namespace chronicle;

class MockEngineRenderer : public Renderer {
  public:
    std::vector<std::string> actions;
    std::vector<std::string> systems;
    std::vector<std::string> errors;
    std::vector<std::string> tokens;
    std::vector<std::string> examined_items;
    std::vector<std::string> resolutions;
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
    void render_resolution(std::string_view narration) override {
        resolutions.push_back(std::string(narration));
    }
    std::string get_player_input(std::string_view) override { return ""; }
    void clear_input_line() override {}
};

class FakeDialogueAgent : public AgentInterface {
  public:
    enum class ChatBehavior { GiveCargoManifest, Remember };

    ToolRegistry *registry = nullptr;
    std::string active_npc_id;
    std::string last_system_prompt;
    std::vector<std::string> system_messages;
    std::string last_user_message;
    std::vector<std::string> user_messages;
    int clear_history_call_count = 0;
    int register_tools_call_count = 0;
    int set_system_prompt_call_count = 0;
    ChatBehavior behavior = ChatBehavior::GiveCargoManifest;
    std::string memory_summary = "Player asked about cargo";
    int memory_importance = 7;

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
        switch (behavior) {
        case ChatBehavior::GiveCargoManifest:
            on_token("Take ");
            poll();
            on_token("this.");
            poll();
            registry->register_give_item(active_npc_id, "cargo_manifest");
            break;
        case ChatBehavior::Remember:
            on_token("I will remember.");
            poll();
            registry->register_add_memory(active_npc_id, memory_summary, memory_importance);
            break;
        }
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

std::string sanitized_temp_name(std::string_view suffix) {
    const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string name = "chronicle_" + std::string(info->test_suite_name()) + "_" + info->name() +
                       "_" + std::string(suffix);
    for (char &ch : name) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
            ch = '_';
        }
    }
    return name;
}

void write_file(const std::filesystem::path &path, std::string_view contents) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write test fixture: " + path.string());
    }
    out << contents;
}

struct TempEngineScenario {
    std::filesystem::path root;
    WorldFileSet files;

    TempEngineScenario() = default;
    explicit TempEngineScenario(std::filesystem::path root_dir) : root(std::move(root_dir)) {
        files = WorldFileSet{.world = root / "world.json",
                             .npcs = root / "npcs.json",
                             .facts = root / "facts.json",
                             .flags = root / "flags.json",
                             .events = root / "events.json"};
    }
    TempEngineScenario(const TempEngineScenario &) = delete;
    TempEngineScenario &operator=(const TempEngineScenario &) = delete;
    TempEngineScenario(TempEngineScenario &&other) noexcept
        : root(std::move(other.root)), files(std::move(other.files)) {
        other.root.clear();
    }
    TempEngineScenario &operator=(TempEngineScenario &&other) noexcept {
        if (this != &other) {
            if (!root.empty()) {
                std::filesystem::remove_all(root);
            }
            root = std::move(other.root);
            files = std::move(other.files);
            other.root.clear();
        }
        return *this;
    }
    ~TempEngineScenario() {
        if (!root.empty()) {
            std::filesystem::remove_all(root);
        }
    }
};

TempEngineScenario make_event_engine_scenario(std::string_view suffix,
                                              std::string_view events_json) {
    auto root = std::filesystem::temp_directory_path() / sanitized_temp_name(suffix);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    TempEngineScenario scenario(root);

    write_file(root / "world.json", R"json({
  "start_location": "start",
  "locations": {
    "start": {
      "name": "Start",
      "base_description": "The starting room.",
      "exits": {"north": "destination"},
      "items": ["takeable_item"],
      "npcs": [],
      "locked_exits": []
    },
    "destination": {
      "name": "Destination",
      "base_description": "The destination room.",
      "exits": {"south": "start"},
      "items": [],
      "npcs": [],
      "locked_exits": []
    }
  },
  "items": {
    "takeable_item": {
      "name": "Takeable Item",
      "description": "An item for event tests.",
      "takeable": true,
      "key_item": false,
      "hidden": false,
      "unlock_target": "",
      "properties": {}
    },
    "spawned_item": {
      "name": "Spawned Item",
      "description": "An item placed by an event.",
      "takeable": true,
      "key_item": false,
      "hidden": false,
      "unlock_target": "",
      "properties": {}
    }
  }
})json");

    write_file(root / "npcs.json", R"json({
  "npcs": {
    "witness": {
      "identity": {
        "id": "witness",
        "name": "Witness",
        "role": "observer",
        "personality_summary": "Precise and quiet.",
        "backstory": "Created for event tests.",
        "secret": "None.",
        "goals": ["Observe events"],
        "knowledge": ["test_fact"],
        "trust_reveal_threshold": 50
      },
      "state": {
        "current_location": "start",
        "mood": "friendly",
        "trust_toward_player": 10,
        "inventory": [],
        "memories": [],
        "has_met_player": false,
        "secret_revealed": false
      }
    }
  }
})json");

    write_file(root / "facts.json", R"json({
  "facts": {
    "test_fact": {
      "text": "A fact used by event tests.",
      "category": "test",
      "revealed_by_default": false
    }
  }
})json");

    write_file(root / "flags.json", R"json({
  "flags": {
    "test_flag": {
      "default": false,
      "description": "A flag used by event tests."
    }
  }
})json");

    write_file(root / "events.json", events_json);

    nlohmann::json config = {
        {"model_path", ""},
        {"turns_per_period", 3},
        {"save_directory", (root / "saves").string()},
    };
    write_file(root / "config.json", config.dump(2));

    nlohmann::json manifest = {
        {"id", "event_test"},
        {"name", "Event Test"},
        {"version", "1.0.0"},
        {"chronicle_schema_version", 1},
        {"files",
         {{"config", "config.json"},
          {"world", "world.json"},
          {"npcs", "npcs.json"},
          {"facts", "facts.json"},
          {"flags", "flags.json"},
          {"events", "events.json"}}},
    };
    write_file(root / "scenario.json", manifest.dump(2));

    return scenario;
}

TempEngineScenario make_locked_exit_engine_scenario(std::string_view suffix) {
    auto root = std::filesystem::temp_directory_path() / sanitized_temp_name(suffix);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    TempEngineScenario scenario(root);

    write_file(root / "world.json", R"json({
  "start_location": "start",
  "locations": {
    "start": {
      "name": "Start",
      "base_description": "A starting room with a locked northern gate.",
      "exits": {"north": "dock", "east": "garden"},
      "items": ["gate_key", "wrong_key"],
      "npcs": [],
      "locked_exits": ["north"]
    },
    "dock": {
      "name": "Tide-Gate Pier",
      "base_description": "A pier behind the locked gate.",
      "exits": {"south": "start"},
      "items": [],
      "npcs": [],
      "locked_exits": []
    },
    "garden": {
      "name": "Garden",
      "base_description": "An unlocked side garden.",
      "exits": {"west": "start"},
      "items": [],
      "npcs": [],
      "locked_exits": []
    }
  },
  "items": {
    "gate_key": {
      "name": "Gate Key",
      "description": "A key for the tide gate.",
      "takeable": true,
      "key_item": true,
      "hidden": false,
      "unlock_target": "dock",
      "properties": {}
    },
    "wrong_key": {
      "name": "Wrong Key",
      "description": "A key for somewhere else.",
      "takeable": true,
      "key_item": true,
      "hidden": false,
      "unlock_target": "garden",
      "properties": {}
    }
  }
})json");

    write_file(root / "npcs.json", R"json({"npcs": {}})json");
    write_file(root / "facts.json", R"json({"facts": {}})json");
    write_file(root / "flags.json", R"json({"flags": {}})json");
    write_file(root / "events.json", R"json({"events": {}})json");

    nlohmann::json config = {
        {"model_path", ""},
        {"turns_per_period", 3},
        {"save_directory", (root / "saves").string()},
    };
    write_file(root / "config.json", config.dump(2));

    return scenario;
}

nlohmann::json event_condition_json(std::string type, std::vector<std::string> args) {
    return nlohmann::json{{"type", std::move(type)}, {"args", std::move(args)}};
}

nlohmann::json event_action_json(std::string type, std::map<std::string, std::string> params = {}) {
    return nlohmann::json{{"type", std::move(type)}, {"params", std::move(params)}};
}

std::string single_event_json(std::vector<nlohmann::json> conditions,
                              std::vector<nlohmann::json> actions, bool once = true) {
    nlohmann::json events = {
        {"events",
         {{"event_under_test",
           {{"conditions", std::move(conditions)},
            {"actions", std::move(actions)},
            {"once", once},
            {"fired", false}}}}},
    };
    return events.dump(2);
}

bool contains_action(const MockEngineRenderer &renderer, std::string_view text) {
    return std::ranges::contains(renderer.actions, std::string(text));
}

bool contains_system_text(const MockEngineRenderer &renderer, std::string_view text) {
    return std::ranges::any_of(renderer.systems, [text](const std::string &system) {
        return system.find(text) != std::string::npos;
    });
}

void trigger_take(GameEngine &engine) {
    ParsedCommand take;
    take.verb = CommandVerb::Take;
    take.primary_arg = "takeable item";
    engine.handle_command(take);
}

void trigger_go(GameEngine &engine, std::string direction) {
    ParsedCommand go;
    go.verb = CommandVerb::Go;
    go.primary_arg = std::move(direction);
    engine.handle_command(go);
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

TEST_F(GameEngineTest, LoadRejectsCorruptedSaveWithoutMutatingWorld) {
    auto save_dir = std::filesystem::path("/tmp/chronicle_saves");
    std::filesystem::remove(save_dir / "slot_3.json");

    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(renderer));

    ParsedCommand take;
    take.verb = CommandVerb::Take;
    take.primary_arg = "test item";
    engine.handle_command(take);
    ASSERT_TRUE(std::ranges::contains(engine.world().player.inventory, "test_item"));

    ParsedCommand save;
    save.verb = CommandVerb::Save;
    save.primary_arg = "3";
    engine.handle_command(save);

    {
        std::ifstream in(save_dir / "slot_3.json");
        auto j = nlohmann::json::parse(in);
        j["world"]["player"]["inventory"] = nlohmann::json::array({"phantom_item"});
        std::ofstream out(save_dir / "slot_3.json");
        out << j.dump(2);
    }

    ParsedCommand load;
    load.verb = CommandVerb::Load;
    load.primary_arg = "3";
    engine.handle_command(load);

    ASSERT_FALSE(renderer_ptr->errors.empty());
    EXPECT_NE(renderer_ptr->errors.back().find("corrupted"), std::string::npos);
    EXPECT_TRUE(std::ranges::contains(engine.world().player.inventory, "test_item"));

    std::filesystem::remove(save_dir / "slot_3.json");
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

TEST_F(GameEngineTest, DialogueRememberToolPersistsMemorySilently) {
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    auto fake_agent = std::make_unique<FakeDialogueAgent>();
    fake_agent->behavior = FakeDialogueAgent::ChatBehavior::Remember;
    fake_agent->memory_summary = "  Player noticed the tide ledger  ";
    fake_agent->memory_importance = 15;
    auto pool = std::make_unique<NpcAgentPool>(std::move(fake_agent));
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(renderer), std::move(pool));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "test_npc";
    engine.handle_command(talk);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "Remember this detail.";
    dialogue.primary_arg = "Remember this detail.";
    engine.handle_command(dialogue);

    const auto &memories = engine.world().npcs.at("test_npc").state.memories;
    ASSERT_EQ(memories.size(), 1u);
    EXPECT_EQ(memories[0].summary, "Player noticed the tide ledger");
    EXPECT_EQ(memories[0].importance, 10);
    EXPECT_EQ(memories[0].type, "observation");
    EXPECT_EQ(engine.world().clock.total_turns, 1);
    EXPECT_TRUE(renderer_ptr->actions.empty());
}

TEST_F(GameEngineTest, NpcMemoriesSurviveSaveLoad) {
    auto save_dir = std::filesystem::path("/tmp/chronicle_saves");
    std::filesystem::remove(save_dir / "slot_7.json");

    auto renderer = std::make_unique<MockEngineRenderer>();
    auto fake_agent = std::make_unique<FakeDialogueAgent>();
    auto *fake_agent_ptr = fake_agent.get();
    fake_agent->behavior = FakeDialogueAgent::ChatBehavior::Remember;
    fake_agent->memory_summary = "Player asked about the cargo route";
    fake_agent->memory_importance = 8;
    auto pool = std::make_unique<NpcAgentPool>(std::move(fake_agent));
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(renderer), std::move(pool));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "test_npc";
    engine.handle_command(talk);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "Remember the cargo route.";
    dialogue.primary_arg = "Remember the cargo route.";
    engine.handle_command(dialogue);
    ASSERT_EQ(engine.world().npcs.at("test_npc").state.memories.size(), 1u);

    ParsedCommand save;
    save.verb = CommandVerb::Save;
    save.primary_arg = "7";
    engine.handle_command(save);

    fake_agent_ptr->memory_summary = "Temporary memory after save";
    fake_agent_ptr->memory_importance = 3;
    dialogue.raw_input = "Remember a temporary detail.";
    dialogue.primary_arg = "Remember a temporary detail.";
    engine.handle_command(dialogue);
    ASSERT_EQ(engine.world().npcs.at("test_npc").state.memories.size(), 2u);

    ParsedCommand load;
    load.verb = CommandVerb::Load;
    load.primary_arg = "7";
    engine.handle_command(load);

    const auto &memories = engine.world().npcs.at("test_npc").state.memories;
    ASSERT_EQ(memories.size(), 1u);
    EXPECT_EQ(memories[0].summary, "Player asked about the cargo route");
    EXPECT_EQ(memories[0].importance, 8);
    std::filesystem::remove(save_dir / "slot_7.json");
}

TEST_F(GameEngineTest, ReenteringConversationIncludesPriorMemoryInDynamicContext) {
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto fake_agent = std::make_unique<FakeDialogueAgent>();
    auto *fake_agent_ptr = fake_agent.get();
    fake_agent->behavior = FakeDialogueAgent::ChatBehavior::Remember;
    fake_agent->memory_summary = "Player promised to return with proof";
    fake_agent->memory_importance = 9;
    auto pool = std::make_unique<NpcAgentPool>(std::move(fake_agent));
    GameEngine engine((fixture_root() / "config.json").string(), fixture_world_files(),
                      std::move(renderer), std::move(pool));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "test_npc";
    engine.handle_command(talk);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "I will return with proof.";
    dialogue.primary_arg = "I will return with proof.";
    engine.handle_command(dialogue);
    ASSERT_EQ(engine.world().npcs.at("test_npc").state.memories.size(), 1u);

    ParsedCommand bye;
    bye.verb = CommandVerb::Dialogue;
    bye.raw_input = "bye";
    bye.primary_arg = "bye";
    engine.handle_command(bye);

    engine.handle_command(talk);
    dialogue.raw_input = "What do you remember?";
    dialogue.primary_arg = "What do you remember?";
    engine.handle_command(dialogue);

    ASSERT_GE(fake_agent_ptr->system_messages.size(), 2u);
    const auto &context = fake_agent_ptr->system_messages.back();
    EXPECT_NE(context.find("What you remember:"), std::string::npos);
    EXPECT_NE(context.find("Player promised to return with proof"), std::string::npos);
    EXPECT_EQ(fake_agent_ptr->last_user_message.find("Player promised to return with proof"),
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

TEST_F(GameEngineTest, PlayingHelpListsRuntimeCommands) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(mock_renderer));

    ParsedCommand help;
    help.verb = CommandVerb::Help;
    engine.handle_command(help);

    ASSERT_FALSE(renderer->systems.empty());
    const auto &help_text = renderer->systems.back();
    EXPECT_NE(help_text.find("go <direction>"), std::string::npos);
    EXPECT_NE(help_text.find("use <item> on/with <target>"), std::string::npos);
    EXPECT_NE(help_text.find("talk <npc>"), std::string::npos);
    EXPECT_NE(help_text.find("save [slot]"), std::string::npos);
    EXPECT_EQ(help_text.find("give <item>"), std::string::npos);
}

TEST_F(GameEngineTest, ConversationHelpListsDialogueAndHardCommands) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(mock_renderer));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);

    ParsedCommand help;
    help.verb = CommandVerb::Help;
    engine.handle_command(help);

    ASSERT_FALSE(renderer->systems.empty());
    const auto &help_text = renderer->systems.back();
    EXPECT_NE(help_text.find("type a message"), std::string::npos);
    EXPECT_NE(help_text.find("bye/goodbye/leave"), std::string::npos);
    EXPECT_NE(help_text.find("save [slot]"), std::string::npos);
    EXPECT_EQ(help_text.find("go <direction>"), std::string::npos);
}

TEST_F(GameEngineTest, StubDialogueExplainsEmptyModelPath) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(mock_renderer));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);

    EXPECT_TRUE(contains_system_text(*renderer, "stub output"));
    EXPECT_TRUE(contains_system_text(*renderer, "no local model"));
    EXPECT_TRUE(contains_system_text(*renderer, "CONTRIBUTING.md#local-model-paths"));

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "Can you hear me?";
    dialogue.primary_arg = "Can you hear me?";
    engine.handle_command(dialogue);

    ASSERT_FALSE(renderer->systems.empty());
    EXPECT_NE(renderer->systems.back().find("AI dialogue stub"), std::string::npos);
    EXPECT_NE(renderer->systems.back().find("no local model"), std::string::npos);
    EXPECT_NE(renderer->systems.back().find("CONTRIBUTING.md#local-model-paths"),
              std::string::npos);
    EXPECT_EQ(engine.world().clock.total_turns, 1);
}

TEST_F(GameEngineTest, InConversationRejectsDirectGameplayCommands) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(mock_renderer));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    engine.handle_command(talk);
    ASSERT_EQ(engine.phase(), GamePhase::InConversation);

    ParsedCommand take;
    take.verb = CommandVerb::Take;
    take.primary_arg = "cargo manifest";
    engine.handle_command(take);

    ASSERT_FALSE(renderer->errors.empty());
    EXPECT_NE(renderer->errors.back().find("conversation"), std::string::npos);
    EXPECT_EQ(engine.phase(), GamePhase::InConversation);
    EXPECT_FALSE(std::ranges::contains(engine.world().player.inventory, "cargo_manifest"));
    EXPECT_EQ(engine.world().clock.total_turns, 0);
}

TEST_F(GameEngineTest, GiveCommandReportsUnsupportedAction) {
    auto *renderer = mock_renderer.get();
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(mock_renderer));

    ParsedCommand give;
    give.verb = CommandVerb::Give;
    give.primary_arg = "cargo manifest";
    engine.handle_command(give);

    ASSERT_FALSE(renderer->errors.empty());
    EXPECT_NE(renderer->errors.back().find("not supported"), std::string::npos);
    EXPECT_EQ(engine.world().clock.total_turns, 0);
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

TEST_F(GameEngineTest, LockedExitBlocksMovement) {
    auto *renderer = mock_renderer.get();
    auto scenario = make_locked_exit_engine_scenario("locked_exit_blocks_movement");
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(mock_renderer));

    trigger_go(engine, "north");

    EXPECT_EQ(engine.world().player.current_location, "start");
    ASSERT_FALSE(renderer->errors.empty());
    EXPECT_EQ(renderer->errors.back(), "The way north is locked.");
    EXPECT_EQ(engine.world().clock.total_turns, 0);
}

TEST_F(GameEngineTest, UseUnlocksLockedExitByDirectionDestinationIdAndName) {
    for (const auto &target : {"north", "dock", "Tide-Gate Pier"}) {
        auto scenario = make_locked_exit_engine_scenario(std::string("use_unlocks_") + target);
        auto renderer = std::make_unique<MockEngineRenderer>();
        auto *renderer_ptr = renderer.get();
        GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                          std::move(renderer));

        ParsedCommand take;
        take.verb = CommandVerb::Take;
        take.primary_arg = "Gate Key";
        engine.handle_command(take);

        ParsedCommand use;
        use.verb = CommandVerb::Use;
        use.primary_arg = "gate_key";
        use.secondary_arg = target;
        engine.handle_command(use);

        EXPECT_FALSE(
            std::ranges::contains(engine.world().locations.at("start").locked_exits, "north"))
            << target;
        EXPECT_EQ(engine.world().clock.total_turns, 2) << target;
        EXPECT_TRUE(contains_action(*renderer_ptr, "You unlock the north exit.")) << target;
    }
}

TEST_F(GameEngineTest, UseRejectsInvalidAttemptsWithoutUnlocking) {
    {
        auto scenario = make_locked_exit_engine_scenario("use_without_item");
        auto renderer = std::make_unique<MockEngineRenderer>();
        auto *renderer_ptr = renderer.get();
        GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                          std::move(renderer));

        ParsedCommand use;
        use.verb = CommandVerb::Use;
        use.primary_arg = "Gate Key";
        use.secondary_arg = "north";
        engine.handle_command(use);

        ASSERT_FALSE(renderer_ptr->errors.empty());
        EXPECT_EQ(renderer_ptr->errors.back(), "You aren't carrying that.");
        EXPECT_TRUE(
            std::ranges::contains(engine.world().locations.at("start").locked_exits, "north"));
        EXPECT_EQ(engine.world().clock.total_turns, 0);
    }

    {
        auto scenario = make_locked_exit_engine_scenario("use_missing_target");
        auto renderer = std::make_unique<MockEngineRenderer>();
        auto *renderer_ptr = renderer.get();
        GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                          std::move(renderer));

        ParsedCommand take;
        take.verb = CommandVerb::Take;
        take.primary_arg = "Gate Key";
        engine.handle_command(take);

        ParsedCommand use;
        use.verb = CommandVerb::Use;
        use.primary_arg = "Gate Key";
        engine.handle_command(use);

        ASSERT_FALSE(renderer_ptr->errors.empty());
        EXPECT_EQ(renderer_ptr->errors.back(), "Use it on what?");
        EXPECT_TRUE(
            std::ranges::contains(engine.world().locations.at("start").locked_exits, "north"));
        EXPECT_EQ(engine.world().clock.total_turns, 1);
    }

    {
        auto scenario = make_locked_exit_engine_scenario("use_target_not_locked");
        auto renderer = std::make_unique<MockEngineRenderer>();
        auto *renderer_ptr = renderer.get();
        GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                          std::move(renderer));

        ParsedCommand take;
        take.verb = CommandVerb::Take;
        take.primary_arg = "Gate Key";
        engine.handle_command(take);

        ParsedCommand use;
        use.verb = CommandVerb::Use;
        use.primary_arg = "Gate Key";
        use.secondary_arg = "garden";
        engine.handle_command(use);

        ASSERT_FALSE(renderer_ptr->errors.empty());
        EXPECT_EQ(renderer_ptr->errors.back(), "That target is not locked.");
        EXPECT_TRUE(
            std::ranges::contains(engine.world().locations.at("start").locked_exits, "north"));
        EXPECT_EQ(engine.world().clock.total_turns, 1);
    }

    {
        auto scenario = make_locked_exit_engine_scenario("use_wrong_item");
        auto renderer = std::make_unique<MockEngineRenderer>();
        auto *renderer_ptr = renderer.get();
        GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                          std::move(renderer));

        ParsedCommand take;
        take.verb = CommandVerb::Take;
        take.primary_arg = "Wrong Key";
        engine.handle_command(take);

        ParsedCommand use;
        use.verb = CommandVerb::Use;
        use.primary_arg = "Wrong Key";
        use.secondary_arg = "north";
        engine.handle_command(use);

        ASSERT_FALSE(renderer_ptr->errors.empty());
        EXPECT_EQ(renderer_ptr->errors.back(), "That doesn't unlock the north exit.");
        EXPECT_TRUE(
            std::ranges::contains(engine.world().locations.at("start").locked_exits, "north"));
        EXPECT_EQ(engine.world().clock.total_turns, 1);
    }
}

TEST_F(GameEngineTest, UnlockStateSurvivesSaveLoad) {
    auto scenario = make_locked_exit_engine_scenario("unlock_save_load");
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(mock_renderer));

    ParsedCommand take;
    take.verb = CommandVerb::Take;
    take.primary_arg = "Gate Key";
    engine.handle_command(take);

    ParsedCommand use;
    use.verb = CommandVerb::Use;
    use.primary_arg = "Gate Key";
    use.secondary_arg = "north";
    engine.handle_command(use);
    ASSERT_FALSE(std::ranges::contains(engine.world().locations.at("start").locked_exits, "north"));

    ParsedCommand save;
    save.verb = CommandVerb::Save;
    engine.handle_command(save);

    ParsedCommand load;
    load.verb = CommandVerb::Load;
    engine.handle_command(load);

    EXPECT_FALSE(std::ranges::contains(engine.world().locations.at("start").locked_exits, "north"));
    trigger_go(engine, "north");
    EXPECT_EQ(engine.world().player.current_location, "dock");
}

TEST_F(GameEngineTest, LighthouseVeilTideGateUsesAuthoredUnlockData) {
    auto root = std::filesystem::path(CHRONICLE_SOURCE_DIR) / "examples" / "lighthouse_veil";
    WorldFileSet files{.world = root / "world.json",
                       .npcs = root / "npcs.json",
                       .facts = root / "facts.json",
                       .flags = root / "flags.json",
                       .events = root / "events.json"};
    GameEngine engine((root / "config.json").string(), files, std::move(mock_renderer));

    const auto &square = engine.world().locations.at("village_square");
    ASSERT_TRUE(std::ranges::contains(square.locked_exits, "west"));
    EXPECT_EQ(square.exits.at("west"), "dock_pier");
    EXPECT_EQ(engine.world().items.at("tide_gate_key").unlock_target, "dock_pier");
}

TEST_F(GameEngineTest, ScriptedEventConditionsHaveTrueAndFalseCases) {
    struct ConditionCase {
        std::string name;
        std::string type;
        std::vector<std::string> true_args;
        std::vector<std::string> false_args;
    };

    const std::vector<ConditionCase> cases = {
        {"clock_is", "clock_is", {"morning"}, {"afternoon"}},
        {"player_at", "player_at", {"start"}, {"destination"}},
        {"flag_set", "flag_set", {"test_flag", "false"}, {"test_flag", "true"}},
        {"npc_trust_ge", "npc_trust_ge", {"witness", "10"}, {"witness", "11"}},
        {"npc_at", "npc_at", {"witness", "start"}, {"witness", "destination"}},
        {"item_in_player_inv", "item_in_player_inv", {"takeable_item"}, {"spawned_item"}},
        {"turn_ge", "turn_ge", {"1"}, {"2"}},
    };

    for (const auto &condition_case : cases) {
        const auto true_events =
            single_event_json({event_condition_json(condition_case.type, condition_case.true_args)},
                              {event_action_json("narrate", {{"text", "condition fired"}})});
        auto true_scenario =
            make_event_engine_scenario("condition_true_" + condition_case.name, true_events);
        auto true_renderer = std::make_unique<MockEngineRenderer>();
        auto *true_renderer_ptr = true_renderer.get();
        GameEngine true_engine((true_scenario.root / "config.json").string(), true_scenario.files,
                               std::move(true_renderer));

        trigger_take(true_engine);

        EXPECT_TRUE(contains_action(*true_renderer_ptr, "condition fired")) << condition_case.name;
        EXPECT_TRUE(true_engine.world().events[0].fired) << condition_case.name;

        const auto false_events = single_event_json(
            {event_condition_json(condition_case.type, condition_case.false_args)},
            {event_action_json("narrate", {{"text", "condition fired"}})});
        auto false_scenario =
            make_event_engine_scenario("condition_false_" + condition_case.name, false_events);
        auto false_renderer = std::make_unique<MockEngineRenderer>();
        auto *false_renderer_ptr = false_renderer.get();
        GameEngine false_engine((false_scenario.root / "config.json").string(),
                                false_scenario.files, std::move(false_renderer));

        trigger_take(false_engine);

        EXPECT_FALSE(contains_action(*false_renderer_ptr, "condition fired"))
            << condition_case.name;
        EXPECT_FALSE(false_engine.world().events[0].fired) << condition_case.name;
    }
}

TEST_F(GameEngineTest, ScriptedEventConditionsUseAndSemantics) {
    const auto events =
        single_event_json({event_condition_json("turn_ge", {"1"}),
                           event_condition_json("player_at", {"destination"})},
                          {event_action_json("narrate", {{"text", "AND event fired."}})});
    auto scenario = make_event_engine_scenario("event_and_semantics", events);
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(renderer));

    trigger_take(engine);

    EXPECT_FALSE(contains_action(*renderer_ptr, "AND event fired."));
    EXPECT_FALSE(engine.world().events[0].fired);
}

TEST_F(GameEngineTest, ScriptedEventActionsUseMutationPipeline) {
    const auto events = single_event_json(
        {event_condition_json("turn_ge", {"1"})},
        {event_action_json("set_flag", {{"flag_id", "test_flag"}, {"value", "true"}}),
         event_action_json("spawn_item",
                           {{"item_id", "spawned_item"}, {"location_id", "destination"}}),
         event_action_json("move_npc", {{"npc_id", "witness"}, {"location_id", "destination"}}),
         event_action_json("narrate", {{"text", "Event actions fired."}})});
    auto scenario = make_event_engine_scenario("event_actions", events);
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(renderer));

    trigger_go(engine, "north");

    EXPECT_TRUE(engine.world().flags.at("test_flag"));
    EXPECT_TRUE(
        std::ranges::contains(engine.world().locations.at("destination").items, "spawned_item"));
    EXPECT_EQ(engine.world().npcs.at("witness").state.current_location, "destination");
    EXPECT_FALSE(std::ranges::contains(engine.world().locations.at("start").npcs, "witness"));
    EXPECT_TRUE(std::ranges::contains(engine.world().locations.at("destination").npcs, "witness"));
    EXPECT_TRUE(engine.world().events[0].fired);
    EXPECT_TRUE(contains_action(*renderer_ptr, "Event actions fired."));
    EXPECT_TRUE(contains_action(*renderer_ptr, "Witness excuses themselves and leaves."));
}

TEST_F(GameEngineTest, ScriptedEventNarrationsRenderInAuthoredOrder) {
    const auto events =
        single_event_json({event_condition_json("turn_ge", {"1"})},
                          {event_action_json("narrate", {{"text", "First event beat."}}),
                           event_action_json("narrate", {{"text", "Second event beat."}})});
    auto scenario = make_event_engine_scenario("event_narration_order", events);
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(renderer));

    trigger_take(engine);

    auto first = std::ranges::find(renderer_ptr->actions, "First event beat.");
    auto second = std::ranges::find(renderer_ptr->actions, "Second event beat.");
    ASSERT_NE(first, renderer_ptr->actions.end());
    ASSERT_NE(second, renderer_ptr->actions.end());
    EXPECT_LT(std::distance(renderer_ptr->actions.begin(), first),
              std::distance(renderer_ptr->actions.begin(), second));
}

TEST_F(GameEngineTest, OneShotScriptedEventsDoNotRefireAfterSaveLoad) {
    const auto events =
        single_event_json({event_condition_json("turn_ge", {"1"})},
                          {event_action_json("narrate", {{"text", "Once event fires."}})});
    auto scenario = make_event_engine_scenario("event_save_load_once", events);
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(renderer));

    trigger_go(engine, "north");
    ASSERT_TRUE(contains_action(*renderer_ptr, "Once event fires."));
    ASSERT_TRUE(engine.world().events[0].fired);

    ParsedCommand save;
    save.verb = CommandVerb::Save;
    engine.handle_command(save);

    ParsedCommand load;
    load.verb = CommandVerb::Load;
    engine.handle_command(load);
    ASSERT_TRUE(engine.world().events[0].fired);

    renderer_ptr->actions.clear();
    trigger_go(engine, "south");

    EXPECT_FALSE(contains_action(*renderer_ptr, "Once event fires."));
    EXPECT_TRUE(engine.world().events[0].fired);
}

TEST_F(GameEngineTest, RepeatingScriptedEventsStayUnfiredAndRefireWhileEligible) {
    const auto events = single_event_json(
        {event_condition_json("turn_ge", {"1"})},
        {event_action_json("narrate", {{"text", "Repeating event fires."}})}, false);
    auto scenario = make_event_engine_scenario("event_repeating", events);
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(renderer));

    trigger_go(engine, "north");
    trigger_go(engine, "south");

    EXPECT_EQ(std::ranges::count(renderer_ptr->actions, "Repeating event fires."), 2);
    EXPECT_FALSE(engine.world().events[0].fired);
}

TEST_F(GameEngineTest, EndGameScriptedEventRendersGenericResolutionAndEntersGameOver) {
    const auto events = single_event_json({event_condition_json("turn_ge", {"1"})},
                                          {event_action_json("end_game")});
    auto scenario = make_event_engine_scenario("event_end_game", events);
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(renderer));

    trigger_go(engine, "north");

    EXPECT_EQ(engine.phase(), GamePhase::GameOver);
    ASSERT_EQ(renderer_ptr->resolutions.size(), 1u);
    EXPECT_EQ(renderer_ptr->resolutions[0], "The scenario has reached its conclusion.");
    EXPECT_TRUE(engine.world().events[0].fired);
}

TEST_F(GameEngineTest, EndGameScriptedEventUsesAuthoredResolutionText) {
    const auto events = single_event_json(
        {event_condition_json("turn_ge", {"1"})},
        {event_action_json("end_game", {{"text", "The authored ending resolves the scenario."}})});
    auto scenario = make_event_engine_scenario("event_end_game_authored_text", events);
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(renderer));

    trigger_go(engine, "north");

    EXPECT_EQ(engine.phase(), GamePhase::GameOver);
    ASSERT_EQ(renderer_ptr->resolutions.size(), 1u);
    EXPECT_EQ(renderer_ptr->resolutions[0], "The authored ending resolves the scenario.");
}

TEST_F(GameEngineTest, GameOverRestrictsCommandsButAllowsHelpLoadAndQuit) {
    const auto events =
        single_event_json({event_condition_json("turn_ge", {"1"})},
                          {event_action_json("end_game", {{"text", "The scenario is over."}})});
    auto scenario = make_event_engine_scenario("game_over_commands", events);
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((scenario.root / "config.json").string(), scenario.files,
                      std::move(renderer));

    ParsedCommand save;
    save.verb = CommandVerb::Save;
    engine.handle_command(save);

    trigger_go(engine, "north");
    ASSERT_EQ(engine.phase(), GamePhase::GameOver);

    ParsedCommand look;
    look.verb = CommandVerb::Look;
    engine.handle_command(look);
    ASSERT_FALSE(renderer_ptr->errors.empty());
    EXPECT_EQ(renderer_ptr->errors.back(),
              "The scenario has ended. Use help, load [slot], or quit.");

    ParsedCommand help;
    help.verb = CommandVerb::Help;
    engine.handle_command(help);
    ASSERT_FALSE(renderer_ptr->systems.empty());
    EXPECT_NE(renderer_ptr->systems.back().find("Commands:"), std::string::npos);

    ParsedCommand load;
    load.verb = CommandVerb::Load;
    engine.handle_command(load);
    EXPECT_EQ(engine.phase(), GamePhase::Playing);

    trigger_go(engine, "north");
    ASSERT_EQ(engine.phase(), GamePhase::GameOver);

    ParsedCommand quit;
    quit.verb = CommandVerb::Quit;
    engine.handle_command(quit);
    ASSERT_FALSE(renderer_ptr->systems.empty());
    EXPECT_EQ(renderer_ptr->systems.back(), "Thanks for playing!");
}

TEST_F(GameEngineTest, BundledSampleReachesEventOnlyEnding) {
    auto renderer = std::make_unique<MockEngineRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine((sample_root() / "config.json").string(), sample_world_files(),
                      std::move(renderer));

    for (const auto &direction : {"north", "south", "north", "south", "north", "south"}) {
        trigger_go(engine, direction);
    }

    EXPECT_EQ(engine.phase(), GamePhase::GameOver);
    ASSERT_EQ(renderer_ptr->resolutions.size(), 1u);
    EXPECT_NE(renderer_ptr->resolutions[0].find("stolen cargo trail goes cold"), std::string::npos);
}
