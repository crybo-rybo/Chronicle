/**
 * @file config.hpp
 * @brief Runtime configuration for Chronicle, loaded from @c config.json.
 *
 * @details The @ref Config struct centralises all tunable parameters so they
 * can be changed without recompilation.  Scenario defaults are loaded once at
 * startup and may be overlaid with machine-local operator overrides before the
 * resolved config is passed down to runtime subsystems.
 *
 * Parameters include model inference settings, prompt-budget limits, timing
 * constants, and renderer preferences.  Any field absent from the JSON file
 * receives its default value defined in the struct.
 */

#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace chronicle {

/// @brief Default per-request inference timeout in milliseconds (two minutes).
inline constexpr int kDefaultInferenceTimeoutMs = 120000;

/// @brief Returns a default set of mutation narration templates.
///
/// @details Used to initialise @ref Config::mutation_narration_templates when
/// neither the config file nor the caller provides custom templates.  The
/// returned map covers all @ref MutationRequest::Type values; some entries have
/// empty strings to indicate that no narration should be rendered for those
/// mutation types.
///
/// @return An @c unordered_map from mutation type key to template string.
std::unordered_map<std::string, std::string> default_mutation_narration_templates();

/// @brief Runtime configuration loaded from @c config.json.
///
/// @details All fields have sane defaults so the game is playable without a
/// config file.  AI-driven dialogue is enabled only when an OpenAI-compatible
/// endpoint URL and model are configured.  Use @ref load to parse a config file
/// and @ref save to write the current configuration back to disk.
struct Config {
    /// @brief Base URL for an OpenAI-compatible chat-completions endpoint.
    ///
    /// Examples: @c "http://localhost:11434/v1" for Ollama or a provider
    /// gateway URL.  Empty disables the agent pool and falls back to stub output.
    std::string llm_base_url;

    /// @brief Model identifier passed to the endpoint.
    ///
    /// Examples: @c "ministral-3:3b", @c "gpt-4o-mini", or a proxy-specific
    /// model alias.  Empty disables the agent pool and falls back to stub output.
    std::string llm_model;

    /// @brief API key for the endpoint.  May be empty for local endpoints.
    std::string llm_api_key;

    /// @brief Optional OpenAI-Organization header value.
    std::string llm_organization;

    /// @brief HTTP request timeout used by the harness transport.  Default: 60000.
    int llm_http_timeout_ms = 60000;

    /// @brief Maximum retry attempts for transient endpoint failures.  Default: 2.
    int llm_max_retries = 2;

    /// @brief Whether to verify TLS certificates for HTTPS endpoints.  Default: true.
    bool llm_tls_verify = true;

    /// @brief Sampling temperature for dialogue generation.  Default: 0.7.
    ///
    /// Higher values increase response creativity; lower values make the model
    /// more deterministic.
    double temperature = 0.7;

    /// @brief Maximum tokens the model may generate per NPC response.  Default: 512.
    int max_response_tokens = 512;

    /// @brief Maximum wall-clock time for one inference request in milliseconds.
    ///
    /// @details Defaults to @ref kDefaultInferenceTimeoutMs (two minutes).  Set
    /// to @c 0 to disable timeout-driven cancellation for local model debugging.
    int inference_timeout_ms = kDefaultInferenceTimeoutMs;

    /// @brief Number of significant player actions per time period.  Default: 5.
    ///
    /// When this count is reached, @ref Clock::advance_turn transitions the
    /// period forward (e.g. Morning → Afternoon).
    int turns_per_period = 5;

    /// @brief Total time periods before the runtime triggers a generic time-expired
    /// ending when no authored @c end_game event fires first.  Default: 12.
    int total_periods = 12;

    /// @brief Token budget allocated to the NPC memory section of the system prompt.  Default: 800.
    ///
    /// @ref PromptBuilder uses this to cap how many @ref MemoryEntry summaries
    /// are included before the prompt is sent to the model.
    int max_memory_tokens = 800;

    /// @brief Token budget allocated to the world-context section of the system prompt.  Default:
    /// 400.
    int max_world_tokens = 400;

    /// @brief Token budget allocated to conversation history in the system prompt.  Default: 600.
    int max_history_tokens = 600;

    /// @brief Directory where save files are written.
    ///
    /// Defaults to @c "saves" relative to the working directory if left empty.
    std::string save_directory;

    /// @brief Enable the TUI renderer if @c true.  Default: @c false (plain terminal).
    bool use_tui = false;

    /// @brief Enable ANSI colour output.  Default: @c true.
    bool use_color = true;

    /// @brief Maximum harness tool-call iterations per request.
    ///
    /// Maps to the harness run iteration and per-run tool-call budgets.  Lower values
    /// reduce wasted context on runaway tool loops; higher values allow more
    /// complex multi-step tool chains.  Default: 5.
    int max_tool_iterations = 5;

    /// @brief Templates for narrating NPC mutation events to the player.
    ///
    /// Keys correspond to @ref MutationRequest::Type string representations
    /// (e.g. @c "give_item_to_player").  Placeholders @c {npc}, @c {item},
    /// @c {mood}, and @c {location} are substituted at runtime.  An empty
    /// string suppresses narration for that mutation type.
    std::unordered_map<std::string, std::string> mutation_narration_templates =
        default_mutation_narration_templates();

    /// @brief Return whether live LLM dialogue has the required endpoint fields.
    bool has_llm_endpoint() const { return !llm_base_url.empty() && !llm_model.empty(); }

    /// @brief Load configuration from a JSON file.
    ///
    /// @details Deserialises the file at @p path using nlohmann/json.  Missing
    /// fields receive their default values.
    ///
    /// @param path Path to the JSON configuration file.
    /// @return A fully populated @ref Config instance.
    /// @throws std::runtime_error if the file cannot be opened or if the JSON
    ///         is malformed.
    static Config load(const std::filesystem::path &path);

    /// @brief Load scenario config and apply local operator overrides.
    ///
    /// @details Precedence is scenario @c config.json, then the optional JSON
    /// file pointed at by @c CHRONICLE_CONFIG_OVERRIDE, then supported
    /// environment variables.  Override JSON is partial: only present fields
    /// replace scenario values, and @c mutation_narration_templates is merged
    /// by key.
    ///
    /// @param path Path to the scenario package config file.
    /// @return A fully resolved @ref Config instance ready for runtime use.
    /// @throws std::runtime_error if the scenario config, override file, or
    ///         environment override values are invalid.
    static Config load_with_operator_overrides(const std::filesystem::path &path);

    /// @brief Write this configuration to a JSON file with 2-space indentation.
    ///
    /// @param path Destination path.  The file is created or overwritten.
    /// @throws std::runtime_error if the file cannot be opened or if the write fails.
    void save(const std::filesystem::path &path) const;
};

/// @cond INTERNAL
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Config, llm_base_url, llm_model, llm_api_key, llm_organization, llm_http_timeout_ms,
    llm_max_retries, llm_tls_verify, temperature, max_response_tokens, inference_timeout_ms,
    turns_per_period, total_periods, max_memory_tokens, max_world_tokens, max_history_tokens,
    save_directory, use_tui, use_color, mutation_narration_templates, max_tool_iterations)
/// @endcond

} // namespace chronicle
