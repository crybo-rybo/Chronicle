#include "entities/cartridge_archive.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace chronicle {
namespace {

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

} // namespace chronicle
