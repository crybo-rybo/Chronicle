#include "entities/cartridge_library.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace chronicle {
namespace {

void write_minimal_cartridge(const std::filesystem::path &root, std::string_view id,
                             std::string_view name) {
    std::filesystem::create_directories(root);
    std::ofstream manifest(root / "scenario.json");
    manifest << R"({
  "id": ")" << id << R"(",
  "name": ")" << name << R"(",
  "version": "1.0.0",
  "chronicle_schema_version": 1
})";
}

} // namespace

TEST(CartridgeLibraryTest, ListDiscoversInstalledCartridge) {
    const auto library_root =
        std::filesystem::temp_directory_path() / "chronicle_library_test" /
        std::to_string(::testing::UnitTest::GetInstance()->random_seed());
    std::filesystem::remove_all(library_root);
    write_minimal_cartridge(library_root / "demo_game", "demo_game", "Demo Game");

    const auto entries = list_cartridges({library_root});
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].id, "demo_game");
    EXPECT_EQ(entries[0].name, "Demo Game");

    std::filesystem::remove_all(library_root);
}

TEST(CartridgeLibraryTest, FindCartridgeById) {
    const auto library_root =
        std::filesystem::temp_directory_path() / "chronicle_library_find_test";
    std::filesystem::remove_all(library_root);
    write_minimal_cartridge(library_root / "find_me", "find_me", "Find Me");

    auto entry = find_cartridge("find_me", {library_root});
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->root_dir.filename(), "find_me");

    std::filesystem::remove_all(library_root);
}

TEST(CartridgeLibraryTest, ResolveExplicitDirectoryPath) {
    const auto dir = std::filesystem::path(CHRONICLE_SOURCE_DIR) / "data";
    auto resolved = resolve_cartridge_path(dir.string());
    ASSERT_TRUE(resolved.has_value());
    EXPECT_TRUE(std::filesystem::exists(*resolved / "scenario.json"));
}

} // namespace chronicle
