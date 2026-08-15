#include <gtest/gtest.h>

#include "chronicle/library.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;
namespace fs = std::filesystem;

TEST(Library, InstallFromDirectoryAndList) {
    ct::TempDir lib("lib");
    ct::TempDir pkg("pkg");
    ct::write_package(pkg.path());

    const auto dest = install_cartridge(pkg.path(), lib.path());
    EXPECT_EQ(dest, lib.path() / "diskworld");
    EXPECT_TRUE(fs::exists(dest / "scenario.json"));

    const auto items = list_cartridges(lib.path());
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items.front().id, "diskworld");
    EXPECT_EQ(items.front().name, "Disk World");
    EXPECT_EQ(items.front().version, "0.1.0");
}

TEST(Library, ReinstallReplacesExisting) {
    ct::TempDir lib("libre");
    ct::TempDir pkg("pkgre");
    ct::write_package(pkg.path());
    (void)install_cartridge(pkg.path(), lib.path());
    ct::write_package(pkg.path(), {{"scenario", {{"version", "0.2.0"}}}});
    (void)install_cartridge(pkg.path(), lib.path());
    const auto items = list_cartridges(lib.path());
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items.front().version, "0.2.0");
}

TEST(Library, InstallRejectsInvalidPackage) {
    ct::TempDir lib("libbad");
    ct::TempDir pkg("pkgbad");
    ct::write_package(pkg.path(), {{"scenario", {{"chronicle_schema_version", 99}}}});
    EXPECT_THROW((void)install_cartridge(pkg.path(), lib.path()), LibraryError);
    EXPECT_TRUE(list_cartridges(lib.path()).empty());
}

TEST(Library, InstallRejectsMissingSource) {
    ct::TempDir lib("libmissing");
    EXPECT_THROW((void)install_cartridge("/nonexistent/nowhere", lib.path()), LibraryError);
}

TEST(Library, PackThenInstallArchiveRoundTrip) {
    ct::TempDir lib("libzip");
    ct::TempDir pkg("pkgzip");
    ct::TempDir out("outzip");
    ct::write_package(pkg.path());

    const auto archive = pack_cartridge(pkg.path(), out.path() / "disk.chronicle");
    EXPECT_TRUE(fs::exists(archive));
    EXPECT_GT(fs::file_size(archive), 0u);

    const auto dest = install_cartridge(archive, lib.path());
    EXPECT_EQ(dest, lib.path() / "diskworld");
    // The installed package still validates and loads.
    EXPECT_FALSE(has_errors(validate_package(dest)));
    // No staging leftovers.
    for (const auto &entry : fs::directory_iterator(lib.path())) {
        EXPECT_FALSE(entry.path().filename().string().starts_with(".staging"));
    }
}

TEST(Library, PackRejectsInvalidPackage) {
    ct::TempDir pkg("pkgpackbad");
    ct::TempDir out("outpackbad");
    ct::write_package(pkg.path(), {{"scenario", {{"chronicle_schema_version", 99}}}});
    EXPECT_THROW((void)pack_cartridge(pkg.path(), out.path() / "bad.chronicle"), LibraryError);
}

TEST(Library, ResolveScenarioByPathIdAndFallback) {
    ct::TempDir lib("libresolve");
    ct::TempDir pkg("pkgresolve");
    ct::write_package(pkg.path());
    (void)install_cartridge(pkg.path(), lib.path());

    // Directory path wins.
    EXPECT_EQ(resolve_scenario(pkg.path().string(), lib.path()), fs::absolute(pkg.path()));
    // Library id.
    EXPECT_EQ(resolve_scenario("diskworld", lib.path()), fs::absolute(lib.path() / "diskworld"));
    // Unknown id throws.
    EXPECT_THROW((void)resolve_scenario("ghostworld", lib.path()), LibraryError);
}

TEST(Library, InspectReportsReadiness) {
    ct::TempDir pkg("pkginspect");
    ct::write_package(pkg.path());
    const auto info = inspect_package(pkg.path());
    EXPECT_EQ(info.id, "diskworld");
    EXPECT_EQ(info.schema, 1);
    EXPECT_TRUE(info.ready);
    EXPECT_TRUE(info.errors.empty());

    ct::TempDir bad("pkginspectbad");
    ct::write_package(bad.path(), {{"scenario", {{"chronicle_schema_version", 99}}}});
    const auto bad_info = inspect_package(bad.path());
    EXPECT_FALSE(bad_info.ready);
    EXPECT_FALSE(bad_info.errors.empty());
}

} // namespace
} // namespace chronicle
