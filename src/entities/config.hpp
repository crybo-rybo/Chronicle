#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace chronicle {

std::unordered_map<std::string, std::string> default_mutation_narration_templates();

/// Runtime configuration loaded from config.json.
struct Config {
    std::string model_path;
    int context_size = 4096;
    int n_gpu_layers = -1;
    double temperature = 0.7;
    int max_response_tokens = 512;
    int turns_per_period = 5;
    int total_periods = 12;
    int max_memory_tokens = 800;
    int max_world_tokens = 400;
    int max_history_tokens = 600;
    std::string save_directory;
    bool use_tui = false;
    bool use_color = true;
    std::unordered_map<std::string, std::string> mutation_narration_templates =
        default_mutation_narration_templates();

    /// Load from JSON file. Throws std::runtime_error if file not found or malformed.
    static Config load(const std::filesystem::path &path);

    /// Save to JSON file with 2-space indentation.
    void save(const std::filesystem::path &path) const;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Config, model_path, context_size, n_gpu_layers,
                                                temperature, max_response_tokens, turns_per_period,
                                                total_periods, max_memory_tokens, max_world_tokens,
                                                max_history_tokens, save_directory, use_tui,
                                                use_color, mutation_narration_templates)

} // namespace chronicle
