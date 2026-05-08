#include "entities/config.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>

namespace chronicle {

// ---------------------------------------------------------------------------
// Load from fixture
// ---------------------------------------------------------------------------

TEST(ConfigTest, LoadFromFixture) {
    Config cfg = Config::load(FIXTURES_DIR "/config.json");
    EXPECT_EQ(cfg.model_path, "/test/model.gguf");
    EXPECT_EQ(cfg.context_size, 2048);
    EXPECT_EQ(cfg.n_gpu_layers, 0);
    EXPECT_DOUBLE_EQ(cfg.temperature, 0.5);
    EXPECT_EQ(cfg.max_response_tokens, 256);
    EXPECT_EQ(cfg.turns_per_period, 3);
    EXPECT_EQ(cfg.total_periods, 8);
    EXPECT_EQ(cfg.max_memory_tokens, 400);
    EXPECT_EQ(cfg.max_world_tokens, 200);
    EXPECT_EQ(cfg.max_history_tokens, 300);
    EXPECT_EQ(cfg.inference_timeout_ms, 1500);
    EXPECT_EQ(cfg.save_directory, "/tmp/chronicle_saves");
    EXPECT_FALSE(cfg.use_tui);
    EXPECT_TRUE(cfg.use_color);
    EXPECT_EQ(cfg.mutation_narration_templates.at("give_item_to_player"),
              "{npc} hands you the {item}.");
}

// ---------------------------------------------------------------------------
// Save + reload roundtrip
// ---------------------------------------------------------------------------

TEST(ConfigTest, SaveReloadRoundtrip) {
    Config original = Config::load(FIXTURES_DIR "/config.json");

    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "chronicle_test_config.json";
    original.save(tmp);

    Config reloaded = Config::load(tmp);

    EXPECT_EQ(reloaded.model_path, original.model_path);
    EXPECT_EQ(reloaded.context_size, original.context_size);
    EXPECT_EQ(reloaded.n_gpu_layers, original.n_gpu_layers);
    EXPECT_DOUBLE_EQ(reloaded.temperature, original.temperature);
    EXPECT_EQ(reloaded.max_response_tokens, original.max_response_tokens);
    EXPECT_EQ(reloaded.turns_per_period, original.turns_per_period);
    EXPECT_EQ(reloaded.total_periods, original.total_periods);
    EXPECT_EQ(reloaded.max_memory_tokens, original.max_memory_tokens);
    EXPECT_EQ(reloaded.max_world_tokens, original.max_world_tokens);
    EXPECT_EQ(reloaded.max_history_tokens, original.max_history_tokens);
    EXPECT_EQ(reloaded.inference_timeout_ms, original.inference_timeout_ms);
    EXPECT_EQ(reloaded.save_directory, original.save_directory);
    EXPECT_EQ(reloaded.use_tui, original.use_tui);
    EXPECT_EQ(reloaded.use_color, original.use_color);
    EXPECT_EQ(reloaded.mutation_narration_templates, original.mutation_narration_templates);

    std::filesystem::remove(tmp);
}

// ---------------------------------------------------------------------------
// Load from nonexistent path throws std::runtime_error
// ---------------------------------------------------------------------------

TEST(ConfigTest, LoadNonexistentThrows) {
    EXPECT_THROW(Config::load("/nonexistent/path/config.json"), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Default construction produces expected defaults
// ---------------------------------------------------------------------------

TEST(ConfigTest, DefaultConstruction) {
    Config cfg;
    EXPECT_TRUE(cfg.model_path.empty());
    EXPECT_EQ(cfg.context_size, 4096);
    EXPECT_EQ(cfg.n_gpu_layers, -1);
    EXPECT_DOUBLE_EQ(cfg.temperature, 0.7);
    EXPECT_EQ(cfg.max_response_tokens, 512);
    EXPECT_EQ(cfg.turns_per_period, 5);
    EXPECT_EQ(cfg.total_periods, 12);
    EXPECT_EQ(cfg.max_memory_tokens, 800);
    EXPECT_EQ(cfg.max_world_tokens, 400);
    EXPECT_EQ(cfg.max_history_tokens, 600);
    EXPECT_EQ(cfg.inference_timeout_ms, 120000);
    EXPECT_TRUE(cfg.save_directory.empty());
    EXPECT_FALSE(cfg.use_tui);
    EXPECT_TRUE(cfg.use_color);
    EXPECT_EQ(cfg.mutation_narration_templates.at("move_npc"),
              "{npc} excuses themselves and leaves.");
}

// ---------------------------------------------------------------------------
// Malformed JSON throws std::runtime_error
// ---------------------------------------------------------------------------

TEST(ConfigTest, LoadMalformedJsonThrows) {
    auto tmp = std::filesystem::temp_directory_path() / "chronicle_test_malformed.json";
    std::FILE *f = std::fopen(tmp.string().c_str(), "w");
    if (f) {
        std::fputs("{bad json", f);
        std::fclose(f);
    }
    EXPECT_THROW(Config::load(tmp), std::runtime_error);
    std::filesystem::remove(tmp);
}

// ---------------------------------------------------------------------------
// max_tool_iterations field
// ---------------------------------------------------------------------------

TEST(ConfigTest, DefaultMaxToolIterations) {
    Config cfg;
    EXPECT_EQ(cfg.max_tool_iterations, 5);
}

TEST(ConfigTest, ZeroInferenceTimeoutDisablesCancellation) {
    Config cfg;
    cfg.inference_timeout_ms = 0;

    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "chronicle_test_config_timeout.json";
    cfg.save(tmp);
    Config reloaded = Config::load(tmp);

    EXPECT_EQ(reloaded.inference_timeout_ms, 0);
    std::filesystem::remove(tmp);
}

TEST(ConfigTest, LoadFromFixtureMaxToolIterations) {
    Config cfg = Config::load(FIXTURES_DIR "/config.json");
    EXPECT_EQ(cfg.max_tool_iterations, 3);
}

TEST(ConfigTest, SaveReloadRoundtripMaxToolIterations) {
    Config original = Config::load(FIXTURES_DIR "/config.json");
    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "chronicle_test_config_mti.json";
    original.save(tmp);
    Config reloaded = Config::load(tmp);
    EXPECT_EQ(reloaded.max_tool_iterations, original.max_tool_iterations);
    std::filesystem::remove(tmp);
}

} // namespace chronicle
