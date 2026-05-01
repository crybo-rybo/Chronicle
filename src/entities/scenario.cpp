/**
 * @file scenario.cpp
 * @brief Implementation of scenario package manifest loading and validation.
 */

#include "entities/scenario.hpp"

#include <fstream>
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

bool require_file(ValidationReport &report, const std::filesystem::path &path,
                  std::string_view label) {
    if (!std::filesystem::exists(path)) {
        add_error(report, std::string(label) + " file does not exist: " + path.string());
        return false;
    }
    if (!std::filesystem::is_regular_file(path)) {
        add_error(report, std::string(label) + " path is not a file: " + path.string());
        return false;
    }
    return true;
}

} // namespace

ScenarioManifest load_scenario_manifest(const std::filesystem::path &scenario_dir) {
    auto manifest_path = scenario_dir / "scenario.json";
    auto json = parse_json_file(manifest_path);
    ScenarioManifest manifest = json.get<ScenarioManifest>();
    if (manifest.chronicle_schema_version != kCurrentScenarioSchemaVersion) {
        throw std::runtime_error("unsupported scenario schema version " +
                                 std::to_string(manifest.chronicle_schema_version) + " in " +
                                 manifest_path.string() + "; expected " +
                                 std::to_string(kCurrentScenarioSchemaVersion));
    }
    return manifest;
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

    try {
        (void)load_world(package.world_files);
    } catch (const std::exception &e) {
        add_error(report, e.what());
    }
    return report;
}

} // namespace chronicle
