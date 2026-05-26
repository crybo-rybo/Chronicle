/**
 * @file config.cpp
 * @brief Implementation of @ref Config load/save and default narration templates.
 */

#include "entities/config.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace chronicle {
namespace {

std::string lower_copy(std::string_view value) {
    std::string result(value);
    std::ranges::transform(result, result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

nlohmann::json parse_json_file(const std::filesystem::path &path, std::string_view label) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Config: cannot open " + std::string(label) +
                                 " file: " + path.string());
    }
    try {
        return nlohmann::json::parse(f);
    } catch (const nlohmann::json::exception &e) {
        throw std::runtime_error("Config: parse error in " + std::string(label) + " file " +
                                 path.string() + ": " + e.what());
    }
}

int parse_int_override(std::string_view source, std::string_view value) {
    try {
        std::size_t consumed = 0;
        int parsed = std::stoi(std::string(value), &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        throw std::runtime_error("Config: " + std::string(source) + " must be an integer");
    }
}

double parse_double_override(std::string_view source, std::string_view value) {
    try {
        std::size_t consumed = 0;
        double parsed = std::stod(std::string(value), &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        throw std::runtime_error("Config: " + std::string(source) + " must be a number");
    }
}

bool parse_bool_override(std::string_view source, std::string_view value) {
    auto normalized = lower_copy(value);
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    throw std::runtime_error("Config: " + std::string(source) +
                             " must be a boolean (true/false, yes/no, on/off, or 1/0)");
}

template <typename T>
void apply_json_field(const nlohmann::json &json, std::string_view field, T &target,
                      std::string_view source) {
    auto field_name = std::string(field);
    if (!json.contains(field_name)) {
        return;
    }
    try {
        target = json.at(field_name).get<T>();
    } catch (const nlohmann::json::exception &e) {
        throw std::runtime_error("Config: invalid override field '" + field_name + "' in " +
                                 std::string(source) + ": " + e.what());
    }
}

void apply_config_json_overrides(Config &config, const nlohmann::json &json,
                                 std::string_view source) {
    if (!json.is_object()) {
        throw std::runtime_error("Config: operator override must be a JSON object: " +
                                 std::string(source));
    }

    apply_json_field(json, "model_path", config.model_path, source);
    apply_json_field(json, "context_size", config.context_size, source);
    apply_json_field(json, "n_gpu_layers", config.n_gpu_layers, source);
    apply_json_field(json, "temperature", config.temperature, source);
    apply_json_field(json, "max_response_tokens", config.max_response_tokens, source);
    apply_json_field(json, "inference_timeout_ms", config.inference_timeout_ms, source);
    apply_json_field(json, "turns_per_period", config.turns_per_period, source);
    apply_json_field(json, "total_periods", config.total_periods, source);
    apply_json_field(json, "max_memory_tokens", config.max_memory_tokens, source);
    apply_json_field(json, "max_world_tokens", config.max_world_tokens, source);
    apply_json_field(json, "max_history_tokens", config.max_history_tokens, source);
    apply_json_field(json, "save_directory", config.save_directory, source);
    apply_json_field(json, "use_tui", config.use_tui, source);
    apply_json_field(json, "use_color", config.use_color, source);
    apply_json_field(json, "max_tool_iterations", config.max_tool_iterations, source);

    if (json.contains("mutation_narration_templates")) {
        const auto &templates = json.at("mutation_narration_templates");
        if (!templates.is_object()) {
            throw std::runtime_error(
                "Config: invalid override field 'mutation_narration_templates' in " +
                std::string(source) + ": must be a JSON object");
        }
        for (const auto &[key, value] : templates.items()) {
            if (!value.is_string()) {
                throw std::runtime_error("Config: mutation_narration_templates.'" + key + "' in " +
                                         std::string(source) + " must be a string");
            }
            config.mutation_narration_templates[key] = value.get<std::string>();
        }
    }
}

void validate_public_config_shape(const nlohmann::json &json, std::string_view source) {
    if (!json.is_object()) {
        throw std::runtime_error("Config: config file must be a JSON object: " +
                                 std::string(source));
    }

    if (!json.contains("verb_aliases")) {
        return;
    }

    const auto &aliases = json.at("verb_aliases");
    if (!aliases.is_object()) {
        throw std::runtime_error("Config: field 'verb_aliases' in " + std::string(source) +
                                 " must be a JSON object");
    }
    for (const auto &[alias, command] : aliases.items()) {
        if (!command.is_string()) {
            throw std::runtime_error("Config: verb_aliases.'" + alias + "' in " +
                                     std::string(source) + " must be a string command");
        }
    }
}

void apply_env_string(const char *name, std::string &target) {
    if (const char *value = std::getenv(name)) {
        target = value;
    }
}

void apply_env_int(const char *name, int &target) {
    if (const char *value = std::getenv(name)) {
        target = parse_int_override(name, value);
    }
}

void apply_env_double(const char *name, double &target) {
    if (const char *value = std::getenv(name)) {
        target = parse_double_override(name, value);
    }
}

void apply_env_bool(const char *name, bool &target) {
    if (const char *value = std::getenv(name)) {
        target = parse_bool_override(name, value);
    }
}

void apply_environment_overrides(Config &config) {
    apply_env_string("ZOO_MODEL_PATH", config.model_path);
    apply_env_string("CHRONICLE_MODEL_PATH", config.model_path);
    apply_env_int("CHRONICLE_CONTEXT_SIZE", config.context_size);
    apply_env_int("CHRONICLE_N_GPU_LAYERS", config.n_gpu_layers);
    apply_env_double("CHRONICLE_TEMPERATURE", config.temperature);
    apply_env_int("CHRONICLE_MAX_RESPONSE_TOKENS", config.max_response_tokens);
    apply_env_int("CHRONICLE_INFERENCE_TIMEOUT_MS", config.inference_timeout_ms);
    apply_env_string("CHRONICLE_SAVE_DIRECTORY", config.save_directory);
    apply_env_bool("CHRONICLE_USE_TUI", config.use_tui);
    apply_env_bool("CHRONICLE_USE_COLOR", config.use_color);
    apply_env_int("CHRONICLE_MAX_TOOL_ITERATIONS", config.max_tool_iterations);
}

} // namespace

std::unordered_map<std::string, std::string> default_mutation_narration_templates() {
    return {
        {"give_item_to_player", "{npc} hands you the {item}."},
        {"take_item_from_player", "{npc} takes the {item}."},
        {"update_npc_mood", "{npc}'s expression shifts - they seem {mood} now."},
        {"move_npc", "{npc} excuses themselves and leaves."},
        {"reveal_knowledge", ""},
        {"update_npc_trust", ""},
        {"add_memory", ""},
        {"set_flag", ""},
    };
}

Config Config::load(const std::filesystem::path &path) {
    try {
        auto json = parse_json_file(path, "config");
        validate_public_config_shape(json, path.string());
        return json.get<Config>();
    } catch (const nlohmann::json::exception &e) {
        throw std::runtime_error("Config: parse error in " + path.string() + ": " + e.what());
    }
}

Config Config::load_with_operator_overrides(const std::filesystem::path &path) {
    Config config = load(path);
    if (const char *override_path = std::getenv("CHRONICLE_CONFIG_OVERRIDE")) {
        if (*override_path != '\0') {
            auto override_json = parse_json_file(override_path, "operator override");
            apply_config_json_overrides(config, override_json, override_path);
        }
    }
    apply_environment_overrides(config);
    return config;
}

void Config::save(const std::filesystem::path &path) const {
    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Config: cannot write file: " + path.string());
    nlohmann::json j = *this;
    f << j.dump(2) << '\n';
    if (!f.good())
        throw std::runtime_error("Config: write failed for " + path.string());
}

} // namespace chronicle
