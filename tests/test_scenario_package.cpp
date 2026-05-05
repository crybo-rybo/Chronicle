#include "entities/scenario.hpp"
#include <array>
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
                    std::string_view world_file_override = "") {
    std::string world_file =
        world_file_override.empty() ? "world.json" : std::string(world_file_override);
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

void make_valid_package(const std::filesystem::path &dir) {
    copy_fixture_files(dir);
    write_manifest(dir);
}

bool has_error_containing(const ValidationReport &report, std::string_view needle) {
    for (const auto &error : report.errors) {
        if (error.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool has_warning_containing(const ValidationReport &report, std::string_view needle) {
    for (const auto &warning : report.warnings) {
        if (warning.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(ScenarioPackageTest, LoadsManifestAndResolvesPackagePaths) {
    auto dir = make_temp_scenario_dir("manifest_load");
    make_valid_package(dir);

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

TEST(ScenarioPackageTest, ValidationReportsMissingScenarioDirectory) {
    auto dir = std::filesystem::temp_directory_path() / "chronicle_missing_scenario_directory";
    std::filesystem::remove_all(dir);

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "scenario directory does not exist"));
    EXPECT_TRUE(has_error_containing(report, dir.string()));
}

TEST(ScenarioPackageTest, ValidationReportsMissingManifest) {
    auto dir = make_temp_scenario_dir("missing_manifest");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "scenario.json"));
}

TEST(ScenarioPackageTest, ValidationReportsMalformedManifestJson) {
    auto dir = make_temp_scenario_dir("malformed_manifest");
    copy_fixture_files(dir);
    write_file(dir / "scenario.json", "{\n");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "failed to parse"));
    EXPECT_TRUE(has_error_containing(report, "scenario.json"));
}

TEST(ScenarioPackageTest, ValidationReportsBadSchemaVersion) {
    auto dir = make_temp_scenario_dir("bad_schema");
    copy_fixture_files(dir);
    write_manifest(dir, 999);

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "schema version"));
    EXPECT_TRUE(has_error_containing(report, "999"));
    EXPECT_TRUE(has_error_containing(report, "expected current version 1"));
}

TEST(ScenarioPackageTest, ValidationReportsManifestIdentityAndMetadataErrors) {
    auto missing_id = make_temp_scenario_dir("missing_manifest_id");
    copy_fixture_files(missing_id);
    write_file(missing_id / "scenario.json", R"json({
  "name": "Test Scenario",
  "version": "0.1.0",
  "chronicle_schema_version": 1
})json");

    auto missing_id_report = validate_scenario_package(missing_id);
    EXPECT_FALSE(missing_id_report.ok);
    EXPECT_TRUE(has_error_containing(missing_id_report, "missing required field 'id'"));

    auto empty_name = make_temp_scenario_dir("empty_manifest_name");
    copy_fixture_files(empty_name);
    write_file(empty_name / "scenario.json", R"json({
  "id": "test_scenario",
  "name": "",
  "version": "0.1.0",
  "chronicle_schema_version": 1
})json");

    auto empty_name_report = validate_scenario_package(empty_name);
    EXPECT_FALSE(empty_name_report.ok);
    EXPECT_TRUE(has_error_containing(empty_name_report, "field 'name' must be non-empty"));

    auto bad_metadata = make_temp_scenario_dir("bad_manifest_metadata");
    copy_fixture_files(bad_metadata);
    write_file(bad_metadata / "scenario.json", R"json({
  "id": "test_scenario",
  "name": "Test Scenario",
  "version": "0.1.0",
  "chronicle_schema_version": 1,
  "metadata": {
    "author": ["not", "a", "string"]
  }
})json");

    auto bad_metadata_report = validate_scenario_package(bad_metadata);
    EXPECT_FALSE(bad_metadata_report.ok);
    EXPECT_TRUE(has_error_containing(bad_metadata_report, "metadata.'author' must be a string"));
}

TEST(ScenarioPackageTest, ValidationReportsMissingReferencedFile) {
    auto dir = make_temp_scenario_dir("missing_referenced_file");
    copy_fixture_files(dir);
    write_manifest(dir, 1, "missing_world.json");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "missing_world.json"));
}

TEST(ScenarioPackageTest, ValidationRejectsAbsoluteManifestPaths) {
    auto dir = make_temp_scenario_dir("absolute_manifest_path");
    copy_fixture_files(dir);
    write_manifest(dir, 1, (dir / "world.json").string());

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "relative"));
}

TEST(ScenarioPackageTest, ValidationRejectsManifestPathsOutsidePackage) {
    auto dir = make_temp_scenario_dir("escaped_manifest_path");
    copy_fixture_files(dir);
    write_manifest(dir, 1, "../world.json");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "package"));
}

TEST(ScenarioPackageTest, ValidationReportsMalformedWorldDataFilesWithFileContext) {
    constexpr std::array<std::string_view, 5> files = {"world.json", "npcs.json", "facts.json",
                                                       "flags.json", "events.json"};

    for (auto file : files) {
        SCOPED_TRACE(file);
        auto dir = make_temp_scenario_dir("malformed_" + std::string(file));
        make_valid_package(dir);
        write_file(dir / file, "{\n");

        auto report = validate_scenario_package(dir);

        EXPECT_FALSE(report.ok);
        EXPECT_TRUE(has_error_containing(report, "failed to parse"));
        EXPECT_TRUE(has_error_containing(report, file));
    }
}

TEST(ScenarioPackageTest, ValidationSurfacesMissingLocationReference) {
    auto dir = make_temp_scenario_dir("missing_location_reference");
    make_valid_package(dir);
    write_file(dir / "npcs.json", R"json({
  "npcs": {
    "test_npc": {
      "identity": {
        "name": "Test NPC",
        "knowledge": ["fact_test"]
      },
      "state": {
        "current_location": "missing_room",
        "mood": "friendly"
      }
    }
  }
})json");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "NPC 'test_npc' current_location 'missing_room'"));
}

TEST(ScenarioPackageTest, ValidationSurfacesDuplicateItemOwnership) {
    auto dir = make_temp_scenario_dir("duplicate_item_ownership");
    make_valid_package(dir);
    write_file(dir / "npcs.json", R"json({
  "npcs": {
    "test_npc": {
      "identity": {
        "name": "Test NPC",
        "knowledge": ["fact_test"]
      },
      "state": {
        "current_location": "test_room",
        "mood": "friendly",
        "inventory": ["test_item"]
      }
    }
  }
})json");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "Item 'test_item' has duplicate ownership"));
}

TEST(ScenarioPackageTest, ValidationSurfacesNpcToolPolicyErrors) {
    auto dir = make_temp_scenario_dir("npc_tool_policy_errors");
    make_valid_package(dir);
    write_file(dir / "npcs.json", R"json({
  "npcs": {
    "test_npc": {
      "identity": {
        "name": "Test NPC",
        "knowledge": ["fact_test"],
        "tool_policy": {
          "allowed_tools": ["say", "invent_magic"],
          "allowed_items": ["missing_item"],
          "allowed_facts": ["missing_fact"],
          "allowed_flags": ["missing_flag"],
          "allowed_locations": ["missing_location"]
        }
      },
      "state": {
        "current_location": "test_room",
        "mood": "friendly"
      }
    }
  }
})json");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "unknown tool 'invent_magic'"));
    EXPECT_TRUE(has_error_containing(report, "allowed_items references missing item"));
    EXPECT_TRUE(has_error_containing(report, "allowed_facts references missing fact"));
    EXPECT_TRUE(has_error_containing(report, "allowed_flags references missing flag"));
    EXPECT_TRUE(has_error_containing(report, "allowed_locations references missing location"));
}

TEST(ScenarioPackageTest, ValidationSurfacesInvalidEventConditionArgumentCount) {
    auto dir = make_temp_scenario_dir("invalid_event_condition_arg_count");
    make_valid_package(dir);
    write_file(dir / "events.json", R"json({
  "events": {
    "bad_condition": {
      "conditions": [
        {"type": "player_at", "args": []}
      ],
      "actions": [
        {"type": "narrate", "params": {"text": "This should not validate."}}
      ]
    }
  }
})json");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "Event 'bad_condition' condition #0 (player_at)"));
    EXPECT_TRUE(has_error_containing(report, "requires 1 arg(s), got 0"));
}

TEST(ScenarioPackageTest, ValidationSurfacesInvalidEventActionType) {
    auto dir = make_temp_scenario_dir("invalid_event_action_type");
    make_valid_package(dir);
    write_file(dir / "events.json", R"json({
  "events": {
    "bad_action": {
      "conditions": [
        {"type": "player_at", "args": ["test_room"]}
      ],
      "actions": [
        {"type": "teleport", "params": {}}
      ]
    }
  }
})json");

    auto report = validate_scenario_package(dir);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "Event 'bad_action' action #0 (teleport)"));
    EXPECT_TRUE(has_error_containing(report, "unknown action type 'teleport'"));
}

TEST(ScenarioPackageTest, ValidationReportsWarningsWithoutFailingValidPackage) {
    auto dir = make_temp_scenario_dir("warning_readable_without_text");
    make_valid_package(dir);
    write_file(dir / "world.json", R"json({
  "start_location": "test_room",
  "locations": {
    "test_room": {
      "name": "Test Room",
      "items": ["test_item"]
    }
  },
  "items": {
    "test_item": {
      "name": "Test Item",
      "properties": {
        "readable": "true"
      }
    }
  }
})json");

    auto report = validate_scenario_package(dir);

    EXPECT_TRUE(report.ok);
    EXPECT_TRUE(report.errors.empty());
    EXPECT_TRUE(has_warning_containing(report, "readable=true"));
    EXPECT_TRUE(has_warning_containing(report, "test_item"));
}

TEST(ScenarioPackageTest, ValidatesBundledSampleScenario) {
    auto report = validate_scenario_package(std::filesystem::path(CHRONICLE_SOURCE_DIR) / "data");

    EXPECT_TRUE(report.ok);
    EXPECT_TRUE(report.errors.empty());
}

} // namespace chronicle
