#include "entities/scenario.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace chronicle {
namespace {

std::filesystem::path make_temp_scenario_dir(std::string_view name) {
    auto dir = std::filesystem::temp_directory_path() / ("chronicle_" + std::string(name));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void write_file(const std::filesystem::path &path, std::string_view contents) {
    std::ofstream out(path);
    out << contents;
}

void copy_fixture_files(const std::filesystem::path &dir) {
    for (std::string_view file :
         {"config.json", "world.json", "npcs.json", "facts.json", "flags.json", "events.json"}) {
        std::filesystem::copy_file(std::filesystem::path(FIXTURES_DIR) / file, dir / file,
                                   std::filesystem::copy_options::overwrite_existing);
    }
}

void write_manifest(const std::filesystem::path &dir, int schema_version = 1,
                    std::string_view missing_file = "") {
    std::string world_file = missing_file.empty() ? "world.json" : std::string(missing_file);
    write_file(dir / "scenario.json", "{\n"
                                      "  \"id\": \"test_scenario\",\n"
                                      "  \"name\": \"Test Scenario\",\n"
                                      "  \"version\": \"0.1.0\",\n"
                                      "  \"chronicle_schema_version\": " +
                                          std::to_string(schema_version) +
                                          ",\n"
                                          "  \"files\": {\n"
                                          "    \"config\": \"config.json\",\n"
                                          "    \"world\": \"" +
                                          world_file +
                                          "\",\n"
                                          "    \"npcs\": \"npcs.json\",\n"
                                          "    \"facts\": \"facts.json\",\n"
                                          "    \"flags\": \"flags.json\",\n"
                                          "    \"events\": \"events.json\"\n"
                                          "  }\n"
                                          "}\n");
}

bool has_error_containing(const ValidationReport &report, std::string_view needle) {
    for (const auto &error : report.errors) {
        if (error.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(ScenarioPackageTest, LoadsManifestAndResolvesPackagePaths) {
    auto dir = make_temp_scenario_dir("manifest_load");
    copy_fixture_files(dir);
    write_manifest(dir);

    auto package = load_scenario_package(dir);

    EXPECT_EQ(package.manifest.id, "test_scenario");
    EXPECT_EQ(package.manifest.name, "Test Scenario");
    EXPECT_EQ(package.manifest.version, "0.1.0");
    EXPECT_EQ(package.config_path, dir / "config.json");
    EXPECT_EQ(package.world_files.world, dir / "world.json");
    EXPECT_EQ(package.world_files.npcs, dir / "npcs.json");
    EXPECT_EQ(package.world_files.facts, dir / "facts.json");
    EXPECT_EQ(package.world_files.flags, dir / "flags.json");
    EXPECT_EQ(package.world_files.events, dir / "events.json");
}

TEST(ScenarioPackageTest, ValidationReportsMissingManifest) {
    auto dir = make_temp_scenario_dir("missing_manifest");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "scenario.json"));
}

TEST(ScenarioPackageTest, ValidationReportsBadSchemaVersion) {
    auto dir = make_temp_scenario_dir("bad_schema");
    copy_fixture_files(dir);
    write_manifest(dir, 999);

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "schema version"));
}

TEST(ScenarioPackageTest, ValidationReportsMissingReferencedFile) {
    auto dir = make_temp_scenario_dir("missing_referenced_file");
    copy_fixture_files(dir);
    write_manifest(dir, 1, "missing_world.json");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "missing_world.json"));
}

TEST(ScenarioPackageTest, ValidatesBundledSampleScenario) {
    auto report = validate_scenario_package(std::filesystem::path(CHRONICLE_SOURCE_DIR) / "data");

    EXPECT_TRUE(report.ok);
    EXPECT_TRUE(report.errors.empty());
}

} // namespace chronicle
