#include "entities/config.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace chronicle {
namespace {

constexpr std::array<const char *, 19> kOperatorEnvNames = {
    "CHRONICLE_CONFIG_OVERRIDE",
    "ZOO_BASE_URL",
    "ZOO_MODEL",
    "ZOO_API_KEY",
    "OPENAI_API_KEY",
    "CHRONICLE_LLM_BASE_URL",
    "CHRONICLE_LLM_MODEL",
    "CHRONICLE_LLM_API_KEY",
    "CHRONICLE_LLM_ORGANIZATION",
    "CHRONICLE_LLM_HTTP_TIMEOUT_MS",
    "CHRONICLE_LLM_MAX_RETRIES",
    "CHRONICLE_LLM_TLS_VERIFY",
    "CHRONICLE_TEMPERATURE",
    "CHRONICLE_MAX_RESPONSE_TOKENS",
    "CHRONICLE_INFERENCE_TIMEOUT_MS",
    "CHRONICLE_SAVE_DIRECTORY",
    "CHRONICLE_USE_TUI",
    "CHRONICLE_USE_COLOR",
    "CHRONICLE_MAX_TOOL_ITERATIONS",
};

class ScopedOperatorEnvironment {
  public:
    ScopedOperatorEnvironment() {
        for (const char *name : kOperatorEnvNames) {
            if (const char *value = std::getenv(name)) {
                previous_.push_back({name, std::string(value)});
            } else {
                previous_.push_back({name, std::nullopt});
            }
            unsetenv(name);
        }
    }

    ~ScopedOperatorEnvironment() {
        for (const auto &entry : previous_) {
            if (entry.value) {
                setenv(entry.name, entry.value->c_str(), 1);
            } else {
                unsetenv(entry.name);
            }
        }
    }

    void set(const char *name, const std::string &value) { setenv(name, value.c_str(), 1); }

  private:
    struct Entry {
        const char *name;
        std::optional<std::string> value;
    };

    std::vector<Entry> previous_;
};

void write_json_file(const std::filesystem::path &path, std::string_view json) {
    std::ofstream out(path);
    out << json;
}

} // namespace

TEST(ConfigTest, LoadFromFixture) {
    Config cfg = Config::load(FIXTURES_DIR "/config.json");
    EXPECT_EQ(cfg.llm_base_url, "http://localhost:11434/v1");
    EXPECT_EQ(cfg.llm_model, "test-model");
    EXPECT_EQ(cfg.llm_api_key, "test-key");
    EXPECT_EQ(cfg.llm_organization, "test-org");
    EXPECT_EQ(cfg.llm_http_timeout_ms, 45000);
    EXPECT_EQ(cfg.llm_max_retries, 1);
    EXPECT_FALSE(cfg.llm_tls_verify);
    EXPECT_TRUE(cfg.has_llm_endpoint());
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

TEST(ConfigTest, SaveReloadRoundtrip) {
    Config original = Config::load(FIXTURES_DIR "/config.json");

    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "chronicle_test_config.json";
    original.save(tmp);

    Config reloaded = Config::load(tmp);

    EXPECT_EQ(reloaded.llm_base_url, original.llm_base_url);
    EXPECT_EQ(reloaded.llm_model, original.llm_model);
    EXPECT_EQ(reloaded.llm_api_key, original.llm_api_key);
    EXPECT_EQ(reloaded.llm_organization, original.llm_organization);
    EXPECT_EQ(reloaded.llm_http_timeout_ms, original.llm_http_timeout_ms);
    EXPECT_EQ(reloaded.llm_max_retries, original.llm_max_retries);
    EXPECT_EQ(reloaded.llm_tls_verify, original.llm_tls_verify);
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

TEST(ConfigTest, LoadNonexistentThrows) {
    EXPECT_THROW(Config::load("/nonexistent/path/config.json"), std::runtime_error);
}

TEST(ConfigTest, DefaultConstruction) {
    Config cfg;
    EXPECT_TRUE(cfg.llm_base_url.empty());
    EXPECT_TRUE(cfg.llm_model.empty());
    EXPECT_TRUE(cfg.llm_api_key.empty());
    EXPECT_TRUE(cfg.llm_organization.empty());
    EXPECT_EQ(cfg.llm_http_timeout_ms, 60000);
    EXPECT_EQ(cfg.llm_max_retries, 2);
    EXPECT_TRUE(cfg.llm_tls_verify);
    EXPECT_FALSE(cfg.has_llm_endpoint());
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

TEST(ConfigTest, HasLlmEndpointRequiresBaseUrlAndModel) {
    Config cfg;
    cfg.llm_base_url = "http://localhost:11434/v1";
    EXPECT_FALSE(cfg.has_llm_endpoint());

    cfg.llm_base_url.clear();
    cfg.llm_model = "test-model";
    EXPECT_FALSE(cfg.has_llm_endpoint());

    cfg.llm_base_url = "http://localhost:11434/v1";
    EXPECT_TRUE(cfg.has_llm_endpoint());
}

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

TEST(ConfigTest, VerbAliasesMustMapToStringCommands) {
    auto tmp = std::filesystem::temp_directory_path() / "chronicle_test_bad_aliases.json";
    write_json_file(tmp, R"({
  "verb_aliases": {
    "inventory": ["bag"]
  }
})");

    EXPECT_THROW(Config::load(tmp), std::runtime_error);
    std::filesystem::remove(tmp);
}

TEST(ConfigTest, RemovedGgufConfigFieldsThrow) {
    for (std::string_view field :
         {"model_path", "context_size", "n_gpu_layers", "auto_configure"}) {
        auto tmp = std::filesystem::temp_directory_path() /
                   ("chronicle_removed_" + std::string(field) + ".json");
        write_json_file(tmp, "{ \"" + std::string(field) + "\": \"removed\" }");

        try {
            (void)Config::load(tmp);
            FAIL() << "Expected removed field to throw: " << field;
        } catch (const std::runtime_error &e) {
            const std::string message = e.what();
            EXPECT_NE(message.find(std::string(field)), std::string::npos);
            EXPECT_NE(message.find("was removed"), std::string::npos);
        }

        std::filesystem::remove(tmp);
    }
}

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

TEST(ConfigTest, OperatorOverrideFileAppliesOnlyPresentFields) {
    ScopedOperatorEnvironment env;
    auto override_path =
        std::filesystem::temp_directory_path() / "chronicle_test_operator_override.json";
    write_json_file(override_path, R"({
  "llm_base_url": "http://override.local/v1",
  "llm_model": "override-model",
  "llm_api_key": "override-key",
  "llm_http_timeout_ms": 30000,
  "max_response_tokens": 64,
  "mutation_narration_templates": {
    "give_item_to_player": "{npc} quietly gives you {item}."
  }
})");
    env.set("CHRONICLE_CONFIG_OVERRIDE", override_path.string());

    Config cfg = Config::load_with_operator_overrides(FIXTURES_DIR "/config.json");

    EXPECT_EQ(cfg.llm_base_url, "http://override.local/v1");
    EXPECT_EQ(cfg.llm_model, "override-model");
    EXPECT_EQ(cfg.llm_api_key, "override-key");
    EXPECT_EQ(cfg.llm_organization, "test-org");
    EXPECT_EQ(cfg.llm_http_timeout_ms, 30000);
    EXPECT_EQ(cfg.llm_max_retries, 1);
    EXPECT_EQ(cfg.max_response_tokens, 64);
    EXPECT_EQ(cfg.turns_per_period, 3);
    EXPECT_EQ(cfg.mutation_narration_templates.at("give_item_to_player"),
              "{npc} quietly gives you {item}.");
    EXPECT_EQ(cfg.mutation_narration_templates.at("move_npc"),
              "{npc} excuses themselves and leaves.");

    std::filesystem::remove(override_path);
}

TEST(ConfigTest, RemovedGgufFieldsInOperatorOverrideThrow) {
    ScopedOperatorEnvironment env;
    auto override_path =
        std::filesystem::temp_directory_path() / "chronicle_test_removed_override.json";
    write_json_file(override_path, R"({
  "model_path": "/local/model.gguf"
})");
    env.set("CHRONICLE_CONFIG_OVERRIDE", override_path.string());

    EXPECT_THROW(Config::load_with_operator_overrides(FIXTURES_DIR "/config.json"),
                 std::runtime_error);

    std::filesystem::remove(override_path);
}

TEST(ConfigTest, EnvironmentOverridesWinOverOperatorOverrideFile) {
    ScopedOperatorEnvironment env;
    auto override_path =
        std::filesystem::temp_directory_path() / "chronicle_test_operator_env_precedence.json";
    write_json_file(override_path, R"({
  "llm_base_url": "http://file.local/v1",
  "llm_model": "file-model",
  "llm_api_key": "file-key",
  "llm_http_timeout_ms": 5000,
  "llm_max_retries": 4,
  "llm_tls_verify": true,
  "inference_timeout_ms": 5000,
  "use_color": true
})");
    env.set("CHRONICLE_CONFIG_OVERRIDE", override_path.string());
    env.set("ZOO_BASE_URL", "http://zoo.local/v1");
    env.set("ZOO_MODEL", "zoo-model");
    env.set("OPENAI_API_KEY", "openai-key");
    env.set("ZOO_API_KEY", "zoo-key");
    env.set("CHRONICLE_LLM_BASE_URL", "http://chronicle.local/v1");
    env.set("CHRONICLE_LLM_MODEL", "chronicle-model");
    env.set("CHRONICLE_LLM_API_KEY", "chronicle-key");
    env.set("CHRONICLE_LLM_ORGANIZATION", "chronicle-org");
    env.set("CHRONICLE_LLM_HTTP_TIMEOUT_MS", "10000");
    env.set("CHRONICLE_LLM_MAX_RETRIES", "0");
    env.set("CHRONICLE_LLM_TLS_VERIFY", "false");
    env.set("CHRONICLE_INFERENCE_TIMEOUT_MS", "0");
    env.set("CHRONICLE_USE_COLOR", "false");

    Config cfg = Config::load_with_operator_overrides(FIXTURES_DIR "/config.json");

    EXPECT_EQ(cfg.llm_base_url, "http://chronicle.local/v1");
    EXPECT_EQ(cfg.llm_model, "chronicle-model");
    EXPECT_EQ(cfg.llm_api_key, "chronicle-key");
    EXPECT_EQ(cfg.llm_organization, "chronicle-org");
    EXPECT_EQ(cfg.llm_http_timeout_ms, 10000);
    EXPECT_EQ(cfg.llm_max_retries, 0);
    EXPECT_FALSE(cfg.llm_tls_verify);
    EXPECT_EQ(cfg.inference_timeout_ms, 0);
    EXPECT_FALSE(cfg.use_color);

    std::filesystem::remove(override_path);
}

TEST(ConfigTest, OpenAiApiKeyFallbackAppliesWhenZooApiKeyUnset) {
    ScopedOperatorEnvironment env;
    env.set("OPENAI_API_KEY", "openai-key");

    Config cfg = Config::load_with_operator_overrides(FIXTURES_DIR "/config.json");

    EXPECT_EQ(cfg.llm_api_key, "openai-key");
}

TEST(ConfigTest, MissingOperatorOverrideFileThrows) {
    ScopedOperatorEnvironment env;
    env.set("CHRONICLE_CONFIG_OVERRIDE", "/nonexistent/chronicle/local_config.json");

    EXPECT_THROW(Config::load_with_operator_overrides(FIXTURES_DIR "/config.json"),
                 std::runtime_error);
}

TEST(ConfigTest, InvalidEnvironmentOverrideThrows) {
    ScopedOperatorEnvironment env;
    env.set("CHRONICLE_MAX_RESPONSE_TOKENS", "many");

    EXPECT_THROW(Config::load_with_operator_overrides(FIXTURES_DIR "/config.json"),
                 std::runtime_error);
}

} // namespace chronicle
