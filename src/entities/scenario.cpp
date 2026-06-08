/**
 * @file scenario.cpp
 * @brief Implementation of scenario package manifest loading and validation.
 */

#include "entities/scenario.hpp"

#include "entities/config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <stdexcept>

namespace chronicle {
namespace {

nlohmann::json parse_json_file(const std::filesystem::path &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open " + path.string());
    }

    try {
        return nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error &e) {
        throw std::runtime_error("failed to parse " + path.string() + ": " + e.what());
    }
}

std::string require_string(const nlohmann::json &json, std::string_view field,
                           std::string_view context) {
    auto field_name = std::string(field);
    if (!json.contains(field_name)) {
        throw std::runtime_error(std::string(context) + " missing required field '" + field_name +
                                 "'");
    }
    if (!json.at(field_name).is_string()) {
        throw std::runtime_error(std::string(context) + " field '" + field_name +
                                 "' must be a string");
    }
    return json.at(field_name).get<std::string>();
}

std::string require_non_empty_string(const nlohmann::json &json, std::string_view field,
                                     std::string_view context) {
    auto value = require_string(json, field, context);
    if (value.empty()) {
        throw std::runtime_error(std::string(context) + " field '" + std::string(field) +
                                 "' must be non-empty");
    }
    return value;
}

void read_optional_path_field(const nlohmann::json &files, std::string_view field,
                              std::string &target) {
    auto field_name = std::string(field);
    if (!files.contains(field_name)) {
        return;
    }
    if (!files.at(field_name).is_string()) {
        throw std::runtime_error("scenario manifest files." + field_name + " must be a string");
    }
    target = files.at(field_name).get<std::string>();
    if (target.empty()) {
        throw std::runtime_error("scenario manifest files." + field_name + " must be non-empty");
    }
}

ScenarioManifest parse_manifest_json(const nlohmann::json &json,
                                     const std::filesystem::path &manifest_path) {
    if (!json.is_object()) {
        throw std::runtime_error("scenario manifest must be a JSON object: " +
                                 manifest_path.string());
    }

    ScenarioManifest manifest;
    manifest.id = require_non_empty_string(json, "id", "scenario manifest");
    manifest.name = require_non_empty_string(json, "name", "scenario manifest");
    manifest.version = require_non_empty_string(json, "version", "scenario manifest");

    if (!json.contains("chronicle_schema_version")) {
        throw std::runtime_error(
            "scenario manifest missing required field 'chronicle_schema_version'");
    }
    if (!json.at("chronicle_schema_version").is_number_integer()) {
        throw std::runtime_error(
            "scenario manifest field 'chronicle_schema_version' must be an integer");
    }
    manifest.chronicle_schema_version = json.at("chronicle_schema_version").get<int>();

    if (manifest.chronicle_schema_version != kCurrentScenarioSchemaVersion) {
        throw std::runtime_error("unsupported scenario schema version " +
                                 std::to_string(manifest.chronicle_schema_version) + " in " +
                                 manifest_path.string() + "; expected current version " +
                                 std::to_string(kCurrentScenarioSchemaVersion));
    }

    if (json.contains("files")) {
        if (!json.at("files").is_object()) {
            throw std::runtime_error("scenario manifest field 'files' must be an object");
        }
        const auto &files = json.at("files");
        read_optional_path_field(files, "config", manifest.files.config);
        read_optional_path_field(files, "world", manifest.files.world);
        read_optional_path_field(files, "npcs", manifest.files.npcs);
        read_optional_path_field(files, "facts", manifest.files.facts);
        read_optional_path_field(files, "flags", manifest.files.flags);
        read_optional_path_field(files, "events", manifest.files.events);
    }

    if (json.contains("metadata")) {
        if (!json.at("metadata").is_object()) {
            throw std::runtime_error("scenario manifest field 'metadata' must be an object");
        }
        for (const auto &[key, value] : json.at("metadata").items()) {
            if (!value.is_string()) {
                throw std::runtime_error("scenario manifest metadata.'" + key +
                                         "' must be a string");
            }
            manifest.metadata[key] = value.get<std::string>();
        }
    }

    return manifest;
}

bool path_is_within(const std::filesystem::path &root, const std::filesystem::path &candidate) {
    auto root_it = root.begin();
    auto candidate_it = candidate.begin();
    for (; root_it != root.end(); ++root_it, ++candidate_it) {
        if (candidate_it == candidate.end() || *root_it != *candidate_it) {
            return false;
        }
    }
    return true;
}

std::filesystem::path resolve_package_path(const std::filesystem::path &root,
                                           std::string_view label, const std::string &path_text) {
    std::filesystem::path path(path_text);
    if (path.is_absolute()) {
        throw std::runtime_error("scenario manifest " + std::string(label) +
                                 " path must be relative: " + path_text);
    }

    auto normalized_root = std::filesystem::absolute(root).lexically_normal();
    auto resolved = (normalized_root / path).lexically_normal();
    if (!path_is_within(normalized_root, resolved)) {
        throw std::runtime_error("scenario manifest " + std::string(label) +
                                 " path must stay inside package: " + path_text);
    }
    return resolved;
}

void add_error(ValidationReport &report, std::string message) {
    report.ok = false;
    report.errors.push_back(std::move(message));
}

void add_warning(ValidationReport &report, std::string message) {
    report.warnings.push_back(std::move(message));
}

bool require_file(ValidationReport &report, const std::filesystem::path &path,
                  std::string_view label) {
    if (!std::filesystem::exists(path)) {
        add_error(report, "scenario manifest " + std::string(label) +
                              " file does not exist: " + path.string());
        return false;
    }
    if (!std::filesystem::is_regular_file(path)) {
        add_error(report, "scenario manifest " + std::string(label) +
                              " path is not a file: " + path.string());
        return false;
    }
    return true;
}

bool has_whitespace_or_path_separator(std::string_view value) {
    return std::ranges::any_of(
        value, [](unsigned char c) { return std::isspace(c) != 0 || c == '/' || c == '\\'; });
}

bool looks_like_simple_semver(const std::string &version) {
    static const std::regex pattern("^([0-9]+)\\.([0-9]+)\\.([0-9]+)"
                                    "([-+][0-9A-Za-z][0-9A-Za-z.-]*)?$");
    return std::regex_match(version, pattern);
}

void add_manifest_readiness_warnings(ValidationReport &report, const ScenarioManifest &manifest) {
    for (const char *key_text : {"description", "author", "license"}) {
        std::string key(key_text);
        if (!manifest.metadata.contains(key)) {
            add_warning(report, "scenario manifest metadata." + key +
                                    " is recommended for shared cartridges");
        }
    }

    if (has_whitespace_or_path_separator(manifest.id)) {
        add_warning(report,
                    "scenario manifest id should not contain whitespace or path separators: " +
                        manifest.id);
    }

    if (!looks_like_simple_semver(manifest.version)) {
        add_warning(
            report,
            "scenario manifest version should look like semantic version major.minor.patch: " +
                manifest.version);
    }
}

bool add_config_readiness_warnings(ValidationReport &report, const std::filesystem::path &path) {
    try {
        auto config = Config::load(path);
        if (!config.llm_base_url.empty() || !config.llm_model.empty()) {
            add_warning(report, "scenario config LLM endpoint fields should be empty in shared "
                                "cartridges; use operator overrides or environment variables");
        }
        if (!config.llm_api_key.empty()) {
            add_warning(report, "scenario config llm_api_key should be empty in shared cartridges; "
                                "never commit endpoint credentials");
        }
        return true;
    } catch (const std::exception &e) {
        add_error(report, e.what());
        return false;
    }
}

} // namespace

ScenarioManifest load_scenario_manifest(const std::filesystem::path &scenario_dir) {
    auto manifest_path = scenario_dir / "scenario.json";
    auto json = parse_json_file(manifest_path);
    return parse_manifest_json(json, manifest_path);
}

ScenarioPackage load_scenario_package(const std::filesystem::path &scenario_dir) {
    ScenarioPackage package;
    package.root_dir = std::filesystem::absolute(scenario_dir).lexically_normal();
    package.manifest = load_scenario_manifest(package.root_dir);
    package.config_path =
        resolve_package_path(package.root_dir, "config", package.manifest.files.config);
    package.world_files.world =
        resolve_package_path(package.root_dir, "world", package.manifest.files.world);
    package.world_files.npcs =
        resolve_package_path(package.root_dir, "npcs", package.manifest.files.npcs);
    package.world_files.facts =
        resolve_package_path(package.root_dir, "facts", package.manifest.files.facts);
    package.world_files.flags =
        resolve_package_path(package.root_dir, "flags", package.manifest.files.flags);
    package.world_files.events =
        resolve_package_path(package.root_dir, "events", package.manifest.files.events);
    return package;
}

ValidationReport validate_scenario_package(const std::filesystem::path &scenario_dir) {
    ValidationReport report;
    if (!std::filesystem::exists(scenario_dir)) {
        add_error(report, "scenario directory does not exist: " + scenario_dir.string());
        return report;
    }
    if (!std::filesystem::is_directory(scenario_dir)) {
        add_error(report, "scenario path is not a directory: " + scenario_dir.string());
        return report;
    }

    auto manifest_path = scenario_dir / "scenario.json";
    if (!std::filesystem::exists(manifest_path)) {
        add_error(report, "scenario manifest does not exist: " + manifest_path.string());
        return report;
    }
    if (!std::filesystem::is_regular_file(manifest_path)) {
        add_error(report, "scenario manifest path is not a file: " + manifest_path.string());
        return report;
    }

    ScenarioPackage package;
    try {
        package = load_scenario_package(scenario_dir);
    } catch (const std::exception &e) {
        add_error(report, e.what());
        return report;
    }

    bool files_ok = true;
    files_ok = require_file(report, package.config_path, "config") && files_ok;
    files_ok = require_file(report, package.world_files.world, "world") && files_ok;
    files_ok = require_file(report, package.world_files.npcs, "npcs") && files_ok;
    files_ok = require_file(report, package.world_files.facts, "facts") && files_ok;
    files_ok = require_file(report, package.world_files.flags, "flags") && files_ok;
    files_ok = require_file(report, package.world_files.events, "events") && files_ok;
    if (!files_ok) {
        return report;
    }

    add_manifest_readiness_warnings(report, package.manifest);
    if (!add_config_readiness_warnings(report, package.config_path)) {
        return report;
    }

    try {
        load_world(package.world_files, &report.warnings);
    } catch (const std::exception &e) {
        add_error(report, e.what());
    }
    return report;
}

} // namespace chronicle
