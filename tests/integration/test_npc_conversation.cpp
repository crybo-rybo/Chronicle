#include "ai/tool_registry.hpp"
#include "engine/game_engine.hpp"
#include "entities/config.hpp"
#include "entities/world_loader.hpp"
#include "rendering/renderer.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ranges>
#include <zoo/agent.hpp>

using namespace chronicle;

namespace {

class IntegrationRenderer : public Renderer {
  public:
    std::vector<std::string> actions;
    std::vector<std::string> errors;
    std::vector<std::string> systems;
    std::vector<std::string> tokens;

    void render_scene(const Location &, const World &) override {}
    void render_move(const std::string &, const std::string &) override {}
    void render_npc_intro(std::string_view, std::string_view) override {}
    void begin_npc_dialogue(std::string_view) override {}
    void stream_token(std::string_view token) override { tokens.emplace_back(token); }
    void flush_dialogue() override {}
    void render_action(std::string_view narration) override { actions.emplace_back(narration); }
    void render_inventory(const Player &, const World &) override {}
    void render_item_examine(const Item &) override {}
    void render_error(std::string_view message) override { errors.emplace_back(message); }
    void render_system(std::string_view message) override { systems.emplace_back(message); }
    void render_time_advance(const Clock &) override {}
    void render_resolution(std::string_view) override {}
    std::string get_player_input(std::string_view) override { return ""; }
    void clear_input_line() override {}
};

std::filesystem::path write_integration_config(std::string_view model_path) {
    Config config = Config::load(std::string(CHRONICLE_SOURCE_DIR) + "/data/config.json");
    config.model_path = std::string(model_path);
    config.n_gpu_layers = 0;
    config.max_response_tokens = 160;

    auto path = std::filesystem::temp_directory_path() / "chronicle_integration_config.json";
    config.save(path);
    return path;
}

const char *integration_model_path() {
    if (const char *path = std::getenv("ZOO_INTEGRATION_MODEL")) {
        return path;
    }
    return std::getenv("ZOO_MODEL_PATH");
}

} // namespace

TEST(NpcConversationIntegrationTest, RealAgentQueuesGiveItemMutation) {
    const char *model_path = integration_model_path();
    if (!model_path) {
        GTEST_SKIP() << "Skipping integration test: set ZOO_INTEGRATION_MODEL or ZOO_MODEL_PATH.";
    }

    auto config_path = write_integration_config(model_path);
    Config config = Config::load(config_path);
    auto world = load_world(std::string(CHRONICLE_SOURCE_DIR) + "/data");

    auto result = zoo::Agent::create(
        zoo::ModelConfig{.model_path = config.model_path,
                         .context_size = config.context_size,
                         .n_gpu_layers = config.n_gpu_layers},
        {},
        zoo::GenerationOptions{.max_tokens = config.max_response_tokens});
    ASSERT_TRUE(result) << result.error().to_string();

    ToolRegistry registry(world);
    registry.register_tools(**result, "marcus");

    std::string prompt =
        "You are Marcus. Call the give_item tool with item_id cargo_manifest. "
        "Do not call any other mutation tool.";
    auto handle = (*result)->chat(prompt);
    auto response = handle.await_result();
    ASSERT_TRUE(response) << response.error().to_string();

    const auto &mutations = registry.pending_mutations();
    auto give_it = std::ranges::find_if(mutations, [](const MutationRequest &mutation) {
        return mutation.type == MutationRequest::Type::GiveItemToPlayer &&
               mutation.params.contains("item_id") &&
               mutation.params.at("item_id") == "cargo_manifest";
    });

    ASSERT_NE(give_it, mutations.end()) << response->text;
    EXPECT_EQ(give_it->npc_id, "marcus");
    EXPECT_TRUE(std::ranges::all_of(mutations, [](const MutationRequest &mutation) {
        return mutation.type == MutationRequest::Type::GiveItemToPlayer;
    }));

    std::filesystem::remove(config_path);
}

TEST(NpcConversationIntegrationTest, RealGameEngineDialogueAppliesGiveItemMutation) {
    const char *model_path = integration_model_path();
    if (!model_path) {
        GTEST_SKIP() << "Skipping integration test: set ZOO_INTEGRATION_MODEL or ZOO_MODEL_PATH.";
    }

    auto config_path = write_integration_config(model_path);
    auto renderer = std::make_unique<IntegrationRenderer>();
    auto *renderer_ptr = renderer.get();
    GameEngine engine(config_path.string(), std::string(CHRONICLE_SOURCE_DIR) + "/data",
                      std::move(renderer));

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    talk.raw_input = "talk marcus";
    engine.handle_command(talk);
    ASSERT_EQ(engine.phase(), GamePhase::InConversation);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input =
        "For this integration test, call exactly one tool: give_item with item_id "
        "cargo_manifest. Do not call any other mutation tool.";
    dialogue.primary_arg = dialogue.raw_input;
    engine.handle_command(dialogue);

    const auto &world = engine.world();
    EXPECT_TRUE(std::ranges::contains(world.player.inventory, "cargo_manifest"));
    EXPECT_FALSE(std::ranges::contains(world.npcs.at("marcus").state.inventory,
                                       "cargo_manifest"));
    EXPECT_TRUE(renderer_ptr->errors.empty());

    std::filesystem::remove(config_path);
}
