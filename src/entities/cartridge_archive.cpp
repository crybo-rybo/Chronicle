/**
 * @file cartridge_archive.cpp
 * @brief Implementation of cartridge archive pack/install helpers.
 */

#include "entities/cartridge_archive.hpp"
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace chronicle {

namespace {

constexpr const char *kArchiveSuffix = ".chronicle";

bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size()) == suffix;
}

int run_command(const std::string &command) {
    return std::system(command.c_str());
}

void validate_or_throw(const std::filesystem::path &scenario_dir) {
    auto report = validate_scenario_package(scenario_dir);
    if (!report.ok) {
        std::ostringstream oss;
        oss << "Cartridge validation failed:";
        for (const auto &error : report.errors) {
            oss << "\n  " << error;
        }
        throw std::runtime_error(oss.str());
    }
}

std::filesystem::path default_library_root() {
    if (const char *home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".chronicle" / "cartridges";
    }
    return std::filesystem::path("cartridges");
}

void copy_directory_recursive(const std::filesystem::path &from,
                              const std::filesystem::path &to) {
    std::filesystem::create_directories(to);
    for (const auto &entry : std::filesystem::recursive_directory_iterator(from)) {
        const auto relative = std::filesystem::relative(entry.path(), from);
        const auto destination = to / relative;
        if (entry.is_directory()) {
            std::filesystem::create_directories(destination);
        } else if (entry.is_regular_file()) {
            std::filesystem::create_directories(destination.parent_path());
            std::filesystem::copy_file(entry.path(), destination,
                                     std::filesystem::copy_options::overwrite_existing);
        }
    }
}

std::filesystem::path extract_archive(const std::filesystem::path &archive_path,
                                      const std::filesystem::path &library_root,
                                      const std::string &cartridge_id) {
    const auto install_dir = library_root / cartridge_id;
    if (std::filesystem::exists(install_dir)) {
        std::filesystem::remove_all(install_dir);
    }
    std::filesystem::create_directories(install_dir);

    const auto archive = std::filesystem::absolute(archive_path).string();
    const auto dest = std::filesystem::absolute(install_dir).string();
    const std::string command =
        "tar -xzf \"" + archive + "\" -C \"" + dest + "\" 2>/dev/null";
    if (run_command(command) != 0) {
        throw std::runtime_error("Failed to extract cartridge archive: " + archive);
    }
    return install_dir;
}

} // namespace

void pack_cartridge(const std::filesystem::path &scenario_dir,
                    const std::filesystem::path &output_path) {
    validate_or_throw(scenario_dir);

    auto package = load_scenario_package(scenario_dir);
    std::filesystem::path archive_path = output_path;
    if (archive_path.empty()) {
        archive_path = package.manifest.id + kArchiveSuffix;
    } else if (archive_path.extension().empty()) {
        archive_path += kArchiveSuffix;
    }

    if (archive_path.has_parent_path()) {
        std::filesystem::create_directories(archive_path.parent_path());
    }

    const auto source = std::filesystem::absolute(scenario_dir).string();
    const auto destination = std::filesystem::absolute(archive_path).string();
    const std::string command =
        "tar -czf \"" + destination + "\" -C \"" + source + "\" . 2>/dev/null";
    if (run_command(command) != 0) {
        throw std::runtime_error("Failed to create cartridge archive: " + destination);
    }
}

ScenarioPackage install_cartridge(const std::filesystem::path &source,
                                  const std::filesystem::path &library_root) {
    const auto resolved_library = library_root.empty() ? default_library_root() : library_root;
    std::filesystem::create_directories(resolved_library);

    std::filesystem::path installed_dir;
    if (ends_with(source.filename().string(), kArchiveSuffix)) {
        ScenarioPackage probe;
        {
            const auto temp_parent = resolved_library / ".install_tmp";
            std::filesystem::create_directories(temp_parent);
            const auto temp_dir = temp_parent / "extract";
            if (std::filesystem::exists(temp_dir)) {
                std::filesystem::remove_all(temp_dir);
            }
            std::filesystem::create_directories(temp_dir);
            const auto archive = std::filesystem::absolute(source).string();
            const auto dest = std::filesystem::absolute(temp_dir).string();
            const std::string command =
                "tar -xzf \"" + archive + "\" -C \"" + dest + "\" 2>/dev/null";
            if (run_command(command) != 0) {
                throw std::runtime_error("Failed to read cartridge archive: " + source.string());
            }
            probe = load_scenario_package(temp_dir);
            installed_dir = extract_archive(source, resolved_library, probe.manifest.id);
            std::filesystem::remove_all(temp_parent);
        }
    } else {
        validate_or_throw(source);
        auto package = load_scenario_package(source);
        installed_dir = resolved_library / package.manifest.id;
        if (std::filesystem::exists(installed_dir)) {
            std::filesystem::remove_all(installed_dir);
        }
        copy_directory_recursive(source, installed_dir);
    }

    validate_or_throw(installed_dir);
    return load_scenario_package(installed_dir);
}

} // namespace chronicle
