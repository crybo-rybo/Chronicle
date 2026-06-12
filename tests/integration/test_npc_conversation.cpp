#include "ai/npc_agent_pool.hpp"
#include "ai/tool_registry.hpp"
#include "engine/game_engine.hpp"
#include "entities/config.hpp"
#include "entities/world_loader.hpp"
#include "rendering/renderer.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <optional>
#include <ranges>
#include <string_view>

using namespace chronicle;

namespace {

auto debug_start_time = std::chrono::steady_clock::now();

void debug_log(std::string_view message) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - debug_start_time);
    std::cerr << "[ChronicleIntegrationDebug +" << elapsed.count() << "ms] " << message
              << std::endl;
}

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

const char *env_first(const char *primary, const char *fallback = nullptr) {
    if (const char *value = std::getenv(primary); value && *value != '\0') {
        return value;
    }
    if (fallback) {
        if (const char *value = std::getenv(fallback); value && *value != '\0') {
            return value;
        }
    }
    return nullptr;
}

std::optional<Config> integration_config() {
    const char *base_url = env_first("CHRONICLE_INTEGRATION_LLM_BASE_URL", "ZOO_BASE_URL");
    const char *model = env_first("CHRONICLE_INTEGRATION_LLM_MODEL", "ZOO_MODEL");
    if (!base_url || !model) {
        return std::nullopt;
    }

    Config config = Config::load(sample_root() / "config.json");
    config.llm_base_url = base_url;
    config.llm_model = model;
    if (const char *api_key = env_first("CHRONICLE_INTEGRATION_LLM_API_KEY", "ZOO_API_KEY")) {
        config.llm_api_key = api_key;
    } else if (const char *openai_key = std::getenv("OPENAI_API_KEY");
               openai_key && *openai_key != '\0') {
        config.llm_api_key = openai_key;
    }
    if (const char *org = env_first("CHRONICLE_INTEGRATION_LLM_ORGANIZATION")) {
        config.llm_organization = org;
    }
    config.max_response_tokens = 64;
    config.max_tool_iterations = 3;
    return config;
}

std::filesystem::path write_integration_config(const Config &config) {
    auto path = std::filesystem::temp_directory_path() / "chronicle_integration_config.json";
    config.save(path);
    debug_log("write_integration_config: wrote temp config");
    return path;
}

} // namespace

TEST(NpcConversationIntegrationTest, RealAgentQueuesGiveItemMutation) {
    debug_start_time = std::chrono::steady_clock::now();
    debug_log("RealAgentQueuesGiveItemMutation: start");
    auto config = integration_config();
    if (!config) {
        GTEST_SKIP() << "Skipping integration test: set CHRONICLE_INTEGRATION_LLM_BASE_URL and "
                        "CHRONICLE_INTEGRATION_LLM_MODEL, or ZOO_BASE_URL and ZOO_MODEL.";
    }
    auto world = load_world(sample_world_files());

    ToolRegistry registry(world);
    auto pool = NpcAgentPool::from_config(*config, registry);
    auto handle = pool.acquire("marcus");
    handle->register_tools(registry, "marcus");
    handle->set_system_prompt("You are Marcus. Use tools exactly as requested for this test.");

    std::size_t token_callbacks = 0;
    auto chat_result = handle->chat_streaming(
        "For this integration test, call exactly one tool: give_item with item_id "
        "cargo_manifest. Do not call any other mutation tool.",
        [&](std::string_view token) {
            ++token_callbacks;
            if (token_callbacks <= 5 || token_callbacks % 16 == 0) {
                debug_log("RealAgentQueuesGiveItemMutation: streaming callback token count=" +
                          std::to_string(token_callbacks) +
                          ", fragment_size=" + std::to_string(token.size()));
            }
        },
        [] {});
    debug_log("RealAgentQueuesGiveItemMutation: chat_streaming returned");
    ASSERT_TRUE(chat_result.success) << chat_result.error_message;

    const auto &mutations = registry.pending_mutations();
    auto give_it = std::ranges::find_if(mutations, [](const MutationRequest &mutation) {
        return mutation.type == MutationRequest::Type::GiveItemToPlayer &&
               mutation.params.contains("item_id") &&
               mutation.params.at("item_id") == "cargo_manifest";
    });

    ASSERT_NE(give_it, mutations.end()) << chat_result.error_message;
    EXPECT_EQ(give_it->actor_id, "marcus");
    EXPECT_TRUE(std::ranges::all_of(mutations, [](const MutationRequest &mutation) {
        return mutation.type == MutationRequest::Type::GiveItemToPlayer;
    }));
}

TEST(NpcConversationIntegrationTest, RealGameEngineDialogueAppliesGiveItemMutation) {
    debug_start_time = std::chrono::steady_clock::now();
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: start");
    auto config = integration_config();
    if (!config) {
        GTEST_SKIP() << "Skipping integration test: set CHRONICLE_INTEGRATION_LLM_BASE_URL and "
                        "CHRONICLE_INTEGRATION_LLM_MODEL, or ZOO_BASE_URL and ZOO_MODEL.";
    }
    auto config_path = write_integration_config(*config);
    auto renderer = std::make_unique<IntegrationRenderer>();
    auto *renderer_ptr = renderer.get();

    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: before GameEngine ctor");
    GameEngine engine(config_path.string(), sample_world_files(), std::move(renderer));
    debug_log("RealGameEngineDialogueAppliesGiveItemMutation: after GameEngine ctor");

    ParsedCommand talk;
    talk.verb = CommandVerb::Talk;
    talk.primary_arg = "marcus";
    talk.raw_input = "talk marcus";
    engine.handle_command(talk);
    ASSERT_EQ(engine.phase(), GamePhase::InConversation);

    ParsedCommand dialogue;
    dialogue.verb = CommandVerb::Dialogue;
    dialogue.raw_input = "For this integration test, call exactly one tool: give_item with item_id "
                         "cargo_manifest. Do not call any other mutation tool.";
    dialogue.primary_arg = dialogue.raw_input;
    engine.handle_command(dialogue);

    const auto &world = engine.world();
    EXPECT_TRUE(std::ranges::contains(world.player.inventory, "cargo_manifest"));
    EXPECT_FALSE(std::ranges::contains(world.npcs.at("marcus").state.inventory, "cargo_manifest"));
    for (const auto &err : renderer_ptr->errors) {
        EXPECT_TRUE(err.find("iteration") != std::string::npos ||
                    err.find("budget") != std::string::npos)
            << "Unexpected error: " << err;
    }

    std::filesystem::remove(config_path);
}
