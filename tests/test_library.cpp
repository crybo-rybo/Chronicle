#include <functional>
#include <gtest/gtest.h>
#include <iterator>
#include <miniz.h>

#include "chronicle/library.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;
namespace fs = std::filesystem;

void write_archive_entry(const fs::path &archive, const std::string &name,
                         const std::string &contents) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, archive.string().c_str(), 0)) {
        throw std::runtime_error("cannot create test archive");
    }
    if (!mz_zip_writer_add_mem(&zip, name.c_str(), contents.data(), contents.size(),
                               MZ_DEFAULT_LEVEL) ||
        !mz_zip_writer_finalize_archive(&zip) || !mz_zip_writer_end(&zip)) {
        throw std::runtime_error("cannot finish test archive");
    }
}

// An archive of `entries`; a name that ends in a slash is a directory.
void write_archive(const fs::path &archive,
                   const std::vector<std::pair<std::string, std::string>> &entries) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, archive.string().c_str(), 0)) {
        throw std::runtime_error("cannot create test archive");
    }
    for (const auto &[name, contents] : entries) {
        if (!mz_zip_writer_add_mem(&zip, name.c_str(), contents.data(), contents.size(),
                                   MZ_DEFAULT_LEVEL)) {
            throw std::runtime_error("cannot add to test archive");
        }
    }
    if (!mz_zip_writer_finalize_archive(&zip) || !mz_zip_writer_end(&zip)) {
        throw std::runtime_error("cannot finish test archive");
    }
}

// The files of a valid package as archive entries under `prefix`.
std::vector<std::pair<std::string, std::string>> package_entries(const std::string &prefix) {
    ct::TempDir pkg("entries");
    ct::write_package(pkg.path());
    std::vector<std::pair<std::string, std::string>> entries;
    for (const auto &file : fs::directory_iterator(pkg.path())) {
        std::ifstream stream(file.path());
        entries.emplace_back(prefix + file.path().filename().string(),
                             std::string(std::istreambuf_iterator<char>(stream), {}));
    }
    return entries;
}

std::size_t entry_count(const fs::path &directory) {
    return static_cast<std::size_t>(
        std::distance(fs::directory_iterator(directory), fs::directory_iterator{}));
}

TEST(Library, InstallFromDirectoryAndList) {
    ct::TempDir lib("lib");
    ct::TempDir pkg("pkg");
    ct::write_package(pkg.path());

    const auto dest = install_cartridge(pkg.path(), lib.path());
    EXPECT_TRUE(fs::equivalent(dest, lib.path() / "diskworld"));
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

TEST(Library, FailedReinstallPreservesExistingCartridge) {
    ct::TempDir lib("libpreserve");
    ct::TempDir pkg("pkgpreserve");
    ct::write_package(pkg.path());
    (void)install_cartridge(pkg.path(), lib.path());

    ct::write_package(pkg.path(),
                      {{"scenario", {{"version", "0.2.0"}, {"chronicle_schema_version", 99}}}});
    EXPECT_THROW((void)install_cartridge(pkg.path(), lib.path()), LibraryError);
    const auto items = list_cartridges(lib.path());
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items.front().version, "0.1.0");
}

TEST(Library, UnsafeIdCannotEscapeLibraryRoot) {
    ct::TempDir lib("libescape");
    ct::TempDir pkg("pkgescape");
    const fs::path protected_file = lib.path() / "protected.txt";
    std::ofstream(protected_file) << "keep";
    ct::write_package(pkg.path(), {{"scenario", {{"id", "../protected"}}}});

    EXPECT_THROW((void)install_cartridge(pkg.path(), lib.path()), LibraryError);
    EXPECT_TRUE(fs::exists(protected_file));
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
    EXPECT_TRUE(fs::equivalent(dest, lib.path() / "diskworld"));
    // The installed package still validates and loads.
    EXPECT_FALSE(has_errors(validate_package(dest)));
    EXPECT_EQ(entry_count(lib.path()), 1u); // No staging or extraction left over.
}

TEST(Library, OrdinaryZipLayoutsInstall) {
    const auto installs = [](const std::vector<std::pair<std::string, std::string>> &entries,
                             const std::string &file_name) {
        ct::TempDir lib("liblayout");
        ct::TempDir source("layout");
        write_archive(source.path() / file_name, entries);
        const auto dest = install_cartridge(source.path() / file_name, lib.path());
        EXPECT_FALSE(has_errors(validate_package(dest)));
        EXPECT_EQ(entry_count(lib.path()), 1u); // No staging or extraction left over.
    };
    installs(package_entries(""), "flat.zip");
    installs(package_entries("disk/"), "SHOUTED.ZIP");

    auto with_directories = package_entries("disk/");
    with_directories.insert(with_directories.begin(), {"disk/", ""});
    with_directories.emplace_back("disk/art/", "");
    installs(with_directories, "directories.zip");

    auto from_macos = package_entries("disk/");
    from_macos.emplace_back("__MACOSX/", "");
    from_macos.emplace_back("__MACOSX/disk/._scenario.json", "x");
    installs(from_macos, "macos.chronicle");
}

TEST(Library, ArchivesWithoutOneCartridgeAreRefused) {
    ct::TempDir lib("libnone");
    ct::TempDir source("none");
    auto two = package_entries("one/");
    for (const auto &entry : package_entries("two/")) {
        two.push_back(entry);
    }
    for (const auto &entries : {two, {{"readme.txt", "hello"}}}) {
        write_archive(source.path() / "a.zip", entries);
        try {
            (void)install_cartridge(source.path() / "a.zip", lib.path());
            ADD_FAILURE() << "installed";
        } catch (const LibraryError &error) {
            EXPECT_STREQ(error.what(), "Archive does not contain one cartridge");
        }
        fs::remove(source.path() / "a.zip");
        EXPECT_EQ(entry_count(lib.path()), 0u);
    }
}

TEST(Library, UnsafeEntriesAreRefusedBeforeExtraction) {
    for (const std::string name : {"../outside.txt", "/outside.txt", "a/../../outside.txt",
                                   "./outside.txt", "a\\outside.txt", "../"}) {
        ct::TempDir lib("libunsafe");
        ct::TempDir source("unsafe");
        const fs::path archive = source.path() / "a.zip";
        // miniz writes no such name, so it is put in place of one it does write.
        const std::string stand_in =
            name.ends_with('/') ? "qq/" : "q" + std::string(name.size() - 1, 'z');
        auto entries = package_entries("");
        entries.emplace_back(stand_in, name.ends_with('/') ? "" : "owned");
        write_archive(archive, entries);
        std::string bytes;
        {
            std::ifstream stream(archive, std::ios::binary);
            bytes.assign(std::istreambuf_iterator<char>(stream), {});
        }
        std::size_t replaced = 0;
        for (auto at = bytes.find(stand_in); at != std::string::npos; at = bytes.find(stand_in)) {
            bytes.replace(at, stand_in.size(), name);
            ++replaced;
        }
        ASSERT_EQ(replaced, 2u) << name; // In its local header and in the directory.
        std::ofstream(archive, std::ios::binary) << bytes;

        try {
            (void)install_cartridge(archive, lib.path());
            ADD_FAILURE() << "installed " << name;
        } catch (const LibraryError &error) {
            EXPECT_NE(std::string(error.what()).find("Unsafe archive entry"), std::string::npos)
                << name << ": " << error.what();
        }
        EXPECT_EQ(entry_count(lib.path()), 0u) << name;
    }
}

// Each limit is the reason given, so each is checked before anything is extracted.
TEST(Library, ArchiveLimitsAreCheckedBeforeExtraction) {
    ct::TempDir lib("liblimits");
    ct::TempDir source("limits");
    const auto refusal = [&](const fs::path &archive) -> std::string {
        try {
            (void)install_cartridge(archive, lib.path());
            return "installed";
        } catch (const LibraryError &error) {
            EXPECT_EQ(entry_count(lib.path()), 0u);
            return error.what();
        }
    };

    auto crowded = package_entries("");
    while (crowded.size() <= MAX_PACKAGE_FILES) {
        crowded.emplace_back("extra_" + std::to_string(crowded.size()), "x");
    }
    write_archive(source.path() / "crowded.zip", crowded);
    EXPECT_EQ(refusal(source.path() / "crowded.zip"), "Archive contains more than 256 files");

    auto heavy = package_entries("");
    for (int index = 0; index < 8; ++index) {
        heavy.emplace_back("blob_" + std::to_string(index),
                           std::string(MAX_PACKAGE_FILE_BYTES, 'z'));
    }
    write_archive(source.path() / "heavy.zip", heavy);
    EXPECT_EQ(refusal(source.path() / "heavy.zip"), "Archive exceeds cartridge extraction limits");

    std::vector<std::pair<std::string, std::string>> countless;
    for (int index = 0; index <= 16'384; ++index) {
        countless.emplace_back(".h" + std::to_string(index), "");
    }
    write_archive(source.path() / "countless.zip", countless);
    EXPECT_EQ(refusal(source.path() / "countless.zip"), "Archive contains more than 16384 entries");

    write_archive_entry(source.path() / "large.zip", "config.json",
                        std::string(MAX_PACKAGE_FILE_BYTES + 1, 'a'));
    EXPECT_EQ(refusal(source.path() / "large.zip"), "Archive exceeds cartridge extraction limits");

    std::ofstream(source.path() / "huge.zip") << "x";
    fs::resize_file(source.path() / "huge.zip", std::uintmax_t{64} * 1024 * 1024 + 1);
    EXPECT_NE(refusal(source.path() / "huge.zip").find("64 MiB input limit"), std::string::npos);
}

// The bytes of an archive of a valid package and one more entry, with the central
// directory record of that entry changed by `change`.
void write_changed_archive(const fs::path &archive, const std::string &last_entry,
                           const std::function<void(std::string &, std::size_t)> &change) {
    auto entries = package_entries("");
    entries.emplace_back(last_entry, "/etc/passwd");
    write_archive(archive, entries);
    std::string bytes;
    {
        std::ifstream stream(archive, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(stream), {});
    }
    const auto record = bytes.rfind(std::string("PK\x01\x02", 4));
    ASSERT_NE(record, std::string::npos);
    change(bytes, record);
    std::ofstream(archive, std::ios::binary) << bytes;
}

void mark_as_link(std::string &bytes, const std::size_t record) {
    bytes[record + 5] = 3; // Made on Unix.
    bytes[record + 40] = '\xFF';
    bytes[record + 41] = '\xA1'; // 0120777 in the high half of the attributes.
}

TEST(Library, LinksAndEncryptedEntriesInArchivesAreRefused) {
    ct::TempDir lib("libziplink");
    ct::TempDir source("ziplink");
    const auto refusal = [&](const auto &change) -> std::string {
        write_changed_archive(source.path() / "a.zip", "entry", change);
        try {
            (void)install_cartridge(source.path() / "a.zip", lib.path());
            return "installed";
        } catch (const LibraryError &error) {
            EXPECT_EQ(entry_count(lib.path()), 0u);
            return error.what();
        }
    };
    EXPECT_EQ(refusal(mark_as_link), "Unsupported archive entry: entry");
    EXPECT_EQ(refusal([](std::string &bytes, const std::size_t record) { bytes[record + 8] |= 1; }),
              "Unsupported archive entry: entry");
    EXPECT_EQ(
        refusal([](std::string &bytes, const std::size_t record) { bytes[record + 10] = 99; }),
        "Unsupported archive entry: entry"); // No such compression method.
}

// What is not part of a package is neither counted nor able to refuse it.
TEST(Library, HiddenEntriesInArchivesAreLeftOut) {
    ct::TempDir lib("libhidden");
    ct::TempDir source("hiddenzip");
    auto entries = package_entries("disk/");
    for (int index = 0; index < 300; ++index) {
        entries.emplace_back("disk/.git/objects/" + std::to_string(index), "x");
        entries.emplace_back("disk/art_" + std::to_string(index) + "/", "");
    }
    entries.emplace_back(".hidden/scenario.json", "{}");
    entries.emplace_back("disk/.blob", std::string(MAX_PACKAGE_FILE_BYTES + 1, 'z'));
    for (int index = 0; index < 100; ++index) {
        entries.emplace_back("disk/note_" + std::to_string(index), "x");
        entries.emplace_back("__MACOSX/disk/._note_" + std::to_string(index), "x");
    }
    write_archive(source.path() / "a.zip", entries);
    const auto dest = install_cartridge(source.path() / "a.zip", lib.path());
    EXPECT_FALSE(fs::exists(dest / ".git"));
    EXPECT_FALSE(fs::exists(dest / ".blob"));
    EXPECT_EQ(inspect_package_tree(dest).files.size(), 107u);

    write_changed_archive(source.path() / "link.zip", ".venv", mark_as_link);
    EXPECT_NO_THROW((void)install_cartridge(source.path() / "link.zip", lib.path()));

    // Hiding does not get an unsafe name past the check.
    write_changed_archive(source.path() / "unsafe.zip", ".q/zz/outside",
                          [](std::string &bytes, std::size_t) {
                              for (auto at = bytes.find(".q/zz/outside"); at != std::string::npos;
                                   at = bytes.find(".q/zz/outside")) {
                                  bytes.replace(at, 13, ".q/../../side");
                              }
                          });
    EXPECT_THROW((void)install_cartridge(source.path() / "unsafe.zip", lib.path()), LibraryError);
}

TEST(Library, PackRefusesNamesAnArchiveCannotHold) {
    ct::TempDir pkg("pkglong");
    ct::TempDir out("outlong");
    ct::write_package(pkg.path());
    fs::path deep = pkg.path();
    for (int index = 0; index < 3; ++index) {
        deep /= std::string(200, static_cast<char>('a' + index));
    }
    fs::create_directories(deep);
    std::ofstream(deep / "note.txt") << "x";
    EXPECT_THROW((void)pack_cartridge(pkg.path(), out.path() / "disk.zip"), LibraryError);
    EXPECT_EQ(entry_count(out.path()), 0u);
}

TEST(Library, OnlyCartridgesInTheirOwnDirectoryAreListed) {
    ct::TempDir lib("libmisplaced");
    ct::TempDir pkg("pkgmisplaced");
    ct::write_package(pkg.path());
    (void)install_cartridge(pkg.path(), lib.path());
    fs::rename(lib.path() / "diskworld", lib.path() / "elsewhere");
    EXPECT_TRUE(list_cartridges(lib.path()).empty());
}

TEST(Library, PackNeverReplacesADirectory) {
    ct::TempDir pkg("pkgdir");
    ct::TempDir out("outdir");
    ct::write_package(pkg.path());
    fs::create_directories(out.path() / "disk.zip");
    EXPECT_THROW((void)pack_cartridge(pkg.path(), out.path() / "disk.zip"), LibraryError);
    EXPECT_TRUE(fs::is_directory(out.path() / "disk.zip"));
    EXPECT_EQ(entry_count(out.path()), 1u);
}

TEST(Library, EntriesThatCollideOnSomeFileSystemsAreRefused) {
    ct::TempDir lib("libcollide");
    ct::TempDir source("collide");
    auto entries = package_entries("");
    entries.emplace_back("WORLD.json", "{}");
    write_archive(source.path() / "a.zip", entries);
    EXPECT_THROW((void)install_cartridge(source.path() / "a.zip", lib.path()), LibraryError);
    EXPECT_EQ(entry_count(lib.path()), 0u);
}

TEST(Library, WhatPackWritesInstallAccepts) {
    ct::TempDir lib("libpack");
    ct::TempDir pkg("pkgpack");
    ct::TempDir out("outpack");
    ct::write_package(pkg.path());
    std::ofstream(pkg.path() / "notes.txt") << std::string(1'000'000, 'z'); // Over 900 to 1.
    fs::create_directories(pkg.path() / ".git");
    std::ofstream(pkg.path() / ".git" / "config") << "private";
    const auto archive = pack_cartridge(pkg.path(), out.path() / "disk.zip");
    const auto dest = install_cartridge(archive, lib.path());
    EXPECT_EQ(fs::file_size(dest / "notes.txt"), 1'000'000u);
    EXPECT_FALSE(fs::exists(dest / ".git"));
    EXPECT_EQ(entry_count(out.path()), 1u);

    // Packing again replaces the archive.
    fs::remove(pkg.path() / "notes.txt");
    const auto before = fs::file_size(archive);
    (void)pack_cartridge(pkg.path(), archive);
    EXPECT_LT(fs::file_size(archive), before);
    EXPECT_EQ(entry_count(out.path()), 1u);
}

TEST(Library, PackRefusesWhatInstallWouldRefuse) {
    ct::TempDir pkg("pkgrefuse");
    ct::TempDir out("outrefuse");
    ct::write_package(pkg.path());
    EXPECT_THROW((void)pack_cartridge(pkg.path(), out.path() / "disk.tar"), LibraryError);
    std::ofstream(pkg.path() / "World.json") << "{}";
    if (entry_count(pkg.path()) == 8u) { // A case-sensitive file system.
        EXPECT_THROW((void)pack_cartridge(pkg.path(), out.path() / "disk.zip"), LibraryError);
    }
    EXPECT_EQ(entry_count(out.path()), 0u);
}

TEST(Library, ReplacingLeavesOneCompleteCartridge) {
    ct::TempDir lib("libswap");
    ct::TempDir pkg("pkgswap");
    ct::write_package(pkg.path());
    std::ofstream(pkg.path() / "only_in_first.txt") << "x";
    (void)install_cartridge(pkg.path(), lib.path());
    fs::remove(pkg.path() / "only_in_first.txt");
    const auto dest = install_cartridge(pkg.path(), lib.path());
    EXPECT_FALSE(fs::exists(dest / "only_in_first.txt"));
    EXPECT_EQ(entry_count(lib.path()), 1u);
}

TEST(Library, ListSkipsWhatItCannotRead) {
    ct::TempDir lib("liblist");
    ct::TempDir pkg("pkglist");
    ct::write_package(pkg.path());
    (void)install_cartridge(pkg.path(), lib.path());
    fs::create_directories(lib.path() / "broken");
    std::ofstream(lib.path() / "broken" / "scenario.json") << "{";
    // Even root cannot look through a link that leads to itself.
    fs::create_directories(lib.path() / "looped");
    fs::create_symlink("scenario.json", lib.path() / "looped" / "scenario.json");
    const auto items = list_cartridges(lib.path());
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items.front().id, "diskworld");
}

TEST(Library, PackRejectsInvalidPackage) {
    ct::TempDir pkg("pkgpackbad");
    ct::TempDir out("outpackbad");
    ct::write_package(pkg.path(), {{"scenario", {{"chronicle_schema_version", 99}}}});
    EXPECT_THROW((void)pack_cartridge(pkg.path(), out.path() / "bad.chronicle"), LibraryError);
}

TEST(Library, PackRejectsOutputInsideSourcePackage) {
    ct::TempDir pkg("pkgselfpack");
    ct::write_package(pkg.path());
    const fs::path output = pkg.path() / "self.chronicle";
    EXPECT_THROW((void)pack_cartridge(pkg.path(), output), LibraryError);
    EXPECT_FALSE(fs::exists(output));
}

TEST(Library, ArchiveTraversalIsRejectedBeforeExtraction) {
    ct::TempDir lib("libzipslip");
    ct::TempDir source("zipslip");
    const fs::path archive = source.path() / "bad.chronicle";
    write_archive_entry(archive, "../outside.txt", "owned");

    EXPECT_THROW((void)install_cartridge(archive, lib.path()), LibraryError);
    EXPECT_FALSE(fs::exists(lib.path() / "outside.txt"));
}

TEST(Library, CompressedOversizedEntryIsRejectedBeforeExtraction) {
    ct::TempDir lib("libzipbomb");
    ct::TempDir source("zipbomb");
    const fs::path archive = source.path() / "bomb.chronicle";
    write_archive_entry(archive, "config.json", std::string(MAX_PACKAGE_FILE_BYTES + 1, 'a'));

    EXPECT_THROW((void)install_cartridge(archive, lib.path()), LibraryError);
    EXPECT_TRUE(list_cartridges(lib.path()).empty());
}

TEST(Library, ResolveScenarioByPathIdAndFallback) {
    ct::TempDir lib("libresolve");
    ct::TempDir pkg("pkgresolve");
    ct::write_package(pkg.path());
    (void)install_cartridge(pkg.path(), lib.path());

    // Directory path wins.
    EXPECT_TRUE(fs::equivalent(resolve_scenario(pkg.path().string(), lib.path()), pkg.path()));
    // Library id.
    EXPECT_TRUE(
        fs::equivalent(resolve_scenario("diskworld", lib.path()), lib.path() / "diskworld"));
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
