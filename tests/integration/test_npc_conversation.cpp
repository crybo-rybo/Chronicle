#include "ai/tool_registry.hpp"
#include "engine/game_engine.hpp"
#include "entities/config.hpp"
#include "entities/world_loader.hpp"
#include "rendering/renderer.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <ranges>
#include <string_view>
#include <zoo/agent.hpp>
#include <zoo/log.hpp>

using namespace chronicle;

namespace {

auto debug_start_time = std::chrono::steady_clock::now();

void debug_log(std::string_view message) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - debug_start_time);
    std::cerr << "[ChronicleIntegrationDebug +" << elapsed.count() << "ms] " << message
              << std::endl;
}

const char *log_level_name(zoo::LogLevel level) {
    switch (level) {
    case zoo::LogLevel::Debug:
        return "debug";
    case zoo::LogLevel::Info:
        return "info";
    case zoo::LogLevel::Warning:
        return "warning";
    case zoo::LogLevel::Error:
        return "error";
    }
    return "unknown";
}

void zoo_debug_log(zoo::LogLevel level, const char *message, void *) {
    std::cerr << "[ZooDebug:" << log_level_name(level) << "] " << message << std::endl;
}

class ZooDebugLogScope {
  public:
    ZooDebugLogScope() { zoo::set_log_callback(zoo_debug_log); }
    ~ZooDebugLogScope() { zoo::reset_log_callback(); }
};

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

std::filesystem::path write_integration_config(std::string_view model_path) {
    debug_log("write_integration_config: loading base scenario config.json");
    Config config = Config::load(sample_root() / "config.json");
    config.model_path = std::string(model_path);
    config.n_gpu_layers = -1;
    config.max_response_tokens = 64;
    debug_log("write_integration_config: overriding n_gpu_layers=-1 and max_response_tokens=64");

    auto path = std::filesystem::temp_directory_path() / "chronicle_integration_config.json";
    config.save(path);
    debug_log("write_integration_config: wrote temp config");
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
    ZooDebugLogScope zoo_logs;
    debug_start_time = std::chrono::steady_clock::now();
    debug_log("RealAgentQueuesGiveItemMutation: start");
    const char *model_path = integration_model_path();
    if (!model_path) {
        GTEST_SKIP() << "Skipping integration test: set ZOO_INTEGRATION_MODEL or ZOO_MODEL_PATH.";
    }

    auto config_path = write_integration_config(model_path);
    debug_log("RealAgentQueuesGiveItemMutation: loading temp config");
    Config config = Config::load(config_path);
    debug_log("RealAgentQueuesGiveItemMutation: loading world");
    auto world = load_world(sample_world_files());

    debug_log("RealAgentQueuesGiveItemMutation: before zoo::Agent::create");
    zoo::ModelConfig model_config;
    model_config.model_path = config.model_path;
    model_config.context_size = config.context_size;
    model_config.n_gpu_layers = config.n_gpu_layers;
    zoo::GenerationOptions gen_opts;
    gen_opts.max_tokens = config.max_response_tokens;
    auto result = zoo::Agent::create(model_config, {}, gen_opts);
    ASSERT_TRUE(result) << result.error().to_string();
    debug_log("RealAgentQueuesGiveItemMutation: after zoo::Agent::create");

    ToolRegistry registry(world);
    debug_log("RealAgentQueuesGiveItemMutation: before ToolRegistry::register_tools");
    registry.register_tools(**result, "marcus");
    debug_log("RealAgentQueuesGiveItemMutation: after ToolRegistry::register_tools");

    std::string prompt = "You are Marcus. Call the give_item tool with item_id cargo_manifest. "
                         "Do not call any other mutation tool.";
    debug_log("RealAgentQueuesGiveItemMutation: before agent->chat");
    std::size_t token_callbacks = 0;
    auto handle = (*result)->chat(prompt, {}, [&](std::string_view token) {
        ++token_callbacks;
        if (token_callbacks <= 5 || token_callbacks % 16 == 0) {
            debug_log("RealAgentQueuesGiveItemMutation: streaming callback token count=" +
                      std::to_string(token_callbacks) +
                      ", fragment_size=" + std::to_string(token.size()));
        }
    });
    debug_log("RealAgentQueuesGiveItemMutation: chat queued, awaiting result");
    auto response = handle.await_result();
    debug_log("RealAgentQueuesGiveItemMutation: await_result returned");
    ASSERT_TRUE(response) << response.error().to_string();

    const auto &mutations = registry.pending_mutations();
    auto give_it = std::ranges::find_if(mutations, [](const MutationRequest &mutation) {
        return mutation.type == MutationRequest::Type::GiveItemToPlayer &&
               mutation.params.contains("item_id") &&
               mutation.params.at("item_id") == "cargo_manifest";
    });

    ASSERT_NE(give_it, mutations.end()) << response->text;
    EXPECT_EQ(give_it->actor_id, "marcus");
    EXPECT_TRUE(std::ranges::all_of(mutations, [](const MutationRequest &mutation) {
        return mutation.type == MutationRequest::Type::GiveItemToPlayer;
    }));

    std::filesystem::remove(config_path);
}

TEST(NpcConversationIntegrationTest, RealGameEngineDialogueAppliesGiveItemMutation) {
    ZooDebugLogScope zoo_logs;
    debug_start_time = std::chrono::steady_clock::now();
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: start");
    const char *model_path = integration_model_path();
    if (!model_path) {
        GTEST_SKIP() << "Skipping integration test: set ZOO_INTEGRATION_MODEL or ZOO_MODEL_PATH.";
    }

    auto config_path = write_integration_config(model_path);
    auto renderer = std::make_unique<IntegrationRenderer>();
    auto *renderer_ptr = renderer.get();
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: before GameEngine ctor");
    GameEngine engine(config_path.string(), sample_world_files(), std::move(renderer));
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: after GameEngine ctor");

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    talk.raw_input = "talk marcus";
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: before talk command");
    engine.handle_command(talk);
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: after talk command");
    ASSERT_EQ(engine.phase(), GamePhase::InConversation);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "For this integration test, call exactly one tool: give_item with item_id "
                         "cargo_manifest. Do not call any other mutation tool.";
    dialogue.primary_arg = dialogue.raw_input;
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: before dialogue command");
    engine.handle_command(dialogue);
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: after dialogue command");

    const auto &world = engine.world();
    EXPECT_TRUE(std::ranges::contains(world.player.inventory, "cargo_manifest"));
    EXPECT_FALSE(std::ranges::contains(world.npcs.at("marcus").state.inventory, "cargo_manifest"));
    // The model may hit Zoo-Keeper's tool iteration limit if it calls extra tools
    // beyond give_item. The critical assertion is that give_item was applied.
    for (const auto &err : renderer_ptr->errors) {
        EXPECT_TRUE(err.find("iteration limit") != std::string::npos ||
                    err.find("Tool loop") != std::string::npos)
            << "Unexpected error: " << err;
    }

    std::filesystem::remove(config_path);
}
