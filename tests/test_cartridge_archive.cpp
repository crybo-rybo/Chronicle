#include "entities/cartridge_archive.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace chronicle {
namespace {

void copy_fixture_package(const std::filesystem::path &source,
                          const std::filesystem::path &destination) {
    std::filesystem::create_directories(destination);
    std::filesystem::copy(source, destination,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
}

class CartridgeArchiveTest : public ::testing::Test {
  protected:
    std::filesystem::path workspace_;

    void SetUp() override {
        workspace_ = std::filesystem::temp_directory_path() / "chronicle_archive_test";
        std::filesystem::remove_all(workspace_);
        std::filesystem::create_directories(workspace_);
    }

    void TearDown() override { std::filesystem::remove_all(workspace_); }
};

} // namespace

TEST_F(CartridgeArchiveTest, InstallDirectoryIntoLibrary) {
    const auto source = std::filesystem::path(CHRONICLE_SOURCE_DIR) / "examples/minimal_scenario";
    const auto library_root = workspace_ / "library";

    auto package = install_cartridge(source, library_root);
    EXPECT_EQ(package.manifest.id, "minimal_scenario");
    EXPECT_TRUE(std::filesystem::exists(package.root_dir / "scenario.json"));

    auto report = validate_scenario_package(package.root_dir);
    EXPECT_TRUE(report.ok);
}

TEST_F(CartridgeArchiveTest, InstallDirectoryCopiesOnlyDeclaredPackageFiles) {
    const auto source = workspace_ / "source";
    copy_fixture_package(std::filesystem::path(CHRONICLE_SOURCE_DIR) / "examples/minimal_scenario",
                         source);
    {
        std::ofstream local_note(source / "local_notes.txt");
        local_note << "not part of the cartridge contract";
    }

    const auto library_root = workspace_ / "library";
    auto package = install_cartridge(source, library_root);

    EXPECT_TRUE(std::filesystem::exists(package.root_dir / "scenario.json"));
    EXPECT_FALSE(std::filesystem::exists(package.root_dir / "local_notes.txt"));
}

TEST_F(CartridgeArchiveTest, PackAndInstallArchiveRoundTrip) {
    const auto source = std::filesystem::path(CHRONICLE_SOURCE_DIR) / "examples/minimal_scenario";
    const auto archive_path = workspace_ / "minimal_scenario.chronicle";
    const auto library_root = workspace_ / "library";

    pack_cartridge(source, archive_path);
    ASSERT_TRUE(std::filesystem::exists(archive_path));

    auto package = install_cartridge(archive_path, library_root);
    EXPECT_EQ(package.manifest.id, "minimal_scenario");
    EXPECT_TRUE(std::filesystem::exists(package.root_dir / "world.json"));
}

TEST_F(CartridgeArchiveTest, PackReturnsActualArchivePathWithSuffix) {
    const auto source = std::filesystem::path(CHRONICLE_SOURCE_DIR) / "examples/minimal_scenario";
    const auto requested_path = workspace_ / "minimal_scenario";

    const auto archive_path = pack_cartridge(source, requested_path);

    EXPECT_EQ(archive_path, workspace_ / "minimal_scenario.chronicle");
    EXPECT_TRUE(std::filesystem::exists(archive_path));
}

TEST_F(CartridgeArchiveTest, PackArchiveExcludesUndeclaredFiles) {
    const auto source = workspace_ / "source";
    copy_fixture_package(std::filesystem::path(CHRONICLE_SOURCE_DIR) / "examples/minimal_scenario",
                         source);
    {
        std::ofstream local_note(source / "local_notes.txt");
        local_note << "not part of the cartridge contract";
    }

    const auto archive_path = workspace_ / "minimal_scenario.chronicle";
    const auto library_root = workspace_ / "library";

    pack_cartridge(source, archive_path);
    auto package = install_cartridge(archive_path, library_root);

    EXPECT_TRUE(std::filesystem::exists(package.root_dir / "scenario.json"));
    EXPECT_FALSE(std::filesystem::exists(package.root_dir / "local_notes.txt"));
}

TEST_F(CartridgeArchiveTest, InstallRejectsUnsafeManifestId) {
    const auto source = workspace_ / "source";
    copy_fixture_package(std::filesystem::path(CHRONICLE_SOURCE_DIR) / "examples/minimal_scenario",
                         source);
    std::ofstream manifest(source / "scenario.json");
    manifest << R"({
  "id": "../escape",
  "name": "Unsafe",
  "version": "1.0.0",
  "chronicle_schema_version": 1
})";
    manifest.close();

    EXPECT_THROW(install_cartridge(source, workspace_ / "library"), std::runtime_error);
}

} // namespace chronicle
