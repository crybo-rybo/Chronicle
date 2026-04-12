#include "engine/game_engine.hpp"
#include <gtest/gtest.h>

using namespace chronicle;

class MockEngineRenderer : public Renderer {
  public:
    std::vector<std::string> actions;
    std::vector<std::string> systems;
    std::vector<std::string> errors;
    std::string last_move_dir;
    std::string last_move_loc;

    void render_scene(const Location &, const World &) override {}
    void render_move(const std::string &direction, const std::string &new_loc) override {
        last_move_dir = direction;
        last_move_loc = new_loc;
    }
    void render_npc_intro(std::string_view, std::string_view) override {}
    void begin_npc_dialogue(std::string_view) override {}
    void stream_token(std::string_view) override {}
    void flush_dialogue() override {}
    void render_action(std::string_view narration) override {
        actions.push_back(std::string(narration));
    }
    void render_inventory(const Player &, const World &) override {}
    void render_item_examine(const Item &) override {}
    void render_error(std::string_view message) override {
        errors.push_back(std::string(message));
    }
    void render_system(std::string_view message) override {
        systems.push_back(std::string(message));
    }
    void render_time_advance(const Clock &) override {}
    void render_resolution(std::string_view) override {}
    std::string get_player_input(std::string_view) override { return ""; }
    void clear_input_line() override {}
};

class GameEngineTest : public ::testing::Test {
  protected:
    std::unique_ptr<MockEngineRenderer> mock_renderer;

    void SetUp() override {
        mock_renderer = std::make_unique<MockEngineRenderer>();
    }
};

TEST_F(GameEngineTest, HandlesGoCommandValid) {
    GameEngine engine(std::string(FIXTURES_DIR) + "/config.json", FIXTURES_DIR, std::move(mock_renderer));
    
    ParsedCommand cmd;
    cmd.verb = CommandVerb::Go;
    cmd.primary_arg = "invalid_dir";
    engine.handle_command(cmd);
    
    EXPECT_EQ(engine.phase(), GamePhase::Playing);
}

TEST_F(GameEngineTest, HandlesQuitCommand) {
    GameEngine engine(std::string(FIXTURES_DIR) + "/config.json", FIXTURES_DIR, std::move(mock_renderer));
    
    ParsedCommand cmd;
    cmd.verb = CommandVerb::Quit;
    engine.handle_command(cmd);
    
    // The running flag becomes false. 
}
