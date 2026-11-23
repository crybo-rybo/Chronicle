#include "chronicle/library.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <miniz.h>
#include <set>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "chronicle/cartridge/loader.hpp"
#include "chronicle/paths.hpp"

namespace chronicle {

namespace fs = std::filesystem;

namespace {

constexpr std::uintmax_t MAX_ARCHIVE_BYTES = 64U * 1024U * 1024U;
// Directories and hidden files included.
constexpr mz_uint MAX_ARCHIVE_ENTRIES = 16'384;

std::optional<fs::path> executable_path() {
#ifdef __linux__
    std::error_code ec;
    auto path = fs::canonical("/proc/self/exe", ec);
    if (!ec) {
        return path;
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    (void)_NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        std::error_code ec;
        auto path = fs::weakly_canonical(buffer.data(), ec);
        if (!ec) {
            return path;
        }
    }
#endif
    return std::nullopt;
}

std::optional<fs::path> installed_minimal_example() {
    const auto executable = executable_path();
    if (!executable) {
        return std::nullopt;
    }
    const auto candidate =
        (executable->parent_path() / CHRONICLE_DATA_FROM_BINDIR / "examples" / "minimal")
            .lexically_normal();
    if (fs::is_directory(candidate) && !fs::is_symlink(candidate)) {
        return fs::weakly_canonical(candidate);
    }
    return std::nullopt;
}

class ScopedDirectory {
  public:
    explicit ScopedDirectory(fs::path path) : path_(std::move(path)) {}
    ~ScopedDirectory() {
        if (!path_.empty()) {
            std::error_code ignored;
            fs::remove_all(path_, ignored);
        }
    }

    ScopedDirectory(const ScopedDirectory &) = delete;
    ScopedDirectory &operator=(const ScopedDirectory &) = delete;

    [[nodiscard]] const fs::path &path() const { return path_; }
    void release() { path_.clear(); }

  private:
    fs::path path_;
};

class ZipReader {
  public:
    // Reading the directory of an archive takes memory in proportion to its size, so
    // the size is checked first.
    explicit ZipReader(const fs::path &archive) {
        std::error_code ec;
        const auto archive_size = fs::file_size(archive, ec);
        if (ec || archive_size > MAX_ARCHIVE_BYTES) {
            throw LibraryError("Archive exceeds the 64 MiB input limit: " + archive.string());
        }
        if (!mz_zip_reader_init_file(&zip_, archive.string().c_str(), 0)) {
            throw LibraryError("Cannot open archive: " + archive.string());
        }
        open_ = true;
    }
    ~ZipReader() {
        if (open_) {
            mz_zip_reader_end(&zip_);
        }
    }

    ZipReader(const ZipReader &) = delete;
    ZipReader &operator=(const ZipReader &) = delete;

    [[nodiscard]] mz_zip_archive *get() { return &zip_; }

  private:
    mz_zip_archive zip_{};
    bool open_ = false;
};

fs::path create_unique_directory(const fs::path &parent, const std::string &prefix) {
    static std::atomic_uint64_t sequence = 0;
    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto number = sequence.fetch_add(1, std::memory_order_relaxed);
        const fs::path candidate =
            parent / (prefix + std::to_string(stamp) + "-" + std::to_string(number));
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) {
            return candidate;
        }
        if (ec && ec != std::errc::file_exists) {
            throw LibraryError("Cannot create staging directory: " + ec.message());
        }
    }
    throw LibraryError("Cannot allocate a unique staging directory");
}

// The path an entry extracts to. Zip tools name a directory with a trailing slash.
std::string archive_entry_path(const mz_zip_archive_file_stat &stat) {
    std::string name = stat.m_filename;
    if (stat.m_is_directory && name.size() > 1 && name.ends_with('/')) {
        name.pop_back();
    }
    return name;
}

bool safe_archive_entry(const std::string &name) {
    const fs::path entry(name);
    if (name.empty() || name.contains('\\') || entry.is_absolute() || entry.has_root_name() ||
        entry.has_root_directory()) {
        return false;
    }
    return std::ranges::all_of(
        entry, [](const fs::path &part) { return !part.empty() && part != "." && part != ".."; });
}

bool archive_entry_is_symlink(const mz_zip_archive_file_stat &stat) {
    constexpr mz_uint16 UNIX_HOST = 3;
    constexpr mz_uint32 FILE_TYPE_MASK = 0170000;
    constexpr mz_uint32 SYMBOLIC_LINK = 0120000;
    const auto host = static_cast<mz_uint16>(stat.m_version_made_by >> 8U);
    const auto mode = stat.m_external_attr >> 16U;
    return host == UNIX_HOST && (mode & FILE_TYPE_MASK) == SYMBOLIC_LINK;
}

std::string portable_archive_key(std::string name) {
    std::ranges::transform(name, name.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return name;
}

// Anything at or under a name that starts with a dot is not part of a package.
// Finder adds one such file beside every other.
bool is_hidden_entry(const std::string &name) {
    return std::ranges::any_of(fs::path(name),
                               [](const fs::path &part) { return part.string().starts_with('.'); });
}

struct ArchiveEntry {
    mz_uint index = 0;
    std::string path;
    bool is_directory = false;
};

// What an archive holds of a package, or why it is refused, before anything in it
// is written. The limits are absolute, so how well an entry compresses does not
// matter.
std::vector<ArchiveEntry> checked_entries(ZipReader &reader, const fs::path &archive) {
    const auto count = mz_zip_reader_get_num_files(reader.get());
    if (count > MAX_ARCHIVE_ENTRIES) {
        throw LibraryError("Archive contains more than " + std::to_string(MAX_ARCHIVE_ENTRIES) +
                           " entries");
    }

    std::vector<ArchiveEntry> entries;
    std::size_t files = 0;
    std::uintmax_t total_size = 0;
    std::set<std::string> names;
    for (mz_uint index = 0; index < count; ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(reader.get(), index, &stat)) {
            throw LibraryError("Cannot read archive entry in " + archive.string());
        }
        // miniz cuts a longer name short without a word.
        if (mz_zip_reader_get_filename(reader.get(), index, nullptr, 0) >
            MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE) {
            throw LibraryError("Archive entry name is too long: " + std::string(stat.m_filename));
        }
        const std::string name = archive_entry_path(stat);
        if (!safe_archive_entry(name)) {
            throw LibraryError("Unsafe archive entry: " + name);
        }
        if (is_hidden_entry(name)) {
            continue;
        }
        if (!names.insert(portable_archive_key(fs::path(name).lexically_normal().generic_string()))
                 .second) {
            throw LibraryError("Archive contains a duplicate entry: " + name);
        }
        if (stat.m_is_encrypted || !stat.m_is_supported || archive_entry_is_symlink(stat)) {
            throw LibraryError("Unsupported archive entry: " + name);
        }
        entries.push_back({.index = index, .path = name, .is_directory = stat.m_is_directory != 0});
        if (stat.m_is_directory) {
            continue;
        }
        if (++files > MAX_PACKAGE_FILES) {
            throw LibraryError("Archive contains more than 256 files");
        }
        if (stat.m_uncomp_size > MAX_PACKAGE_FILE_BYTES ||
            total_size > MAX_PACKAGE_TOTAL_BYTES - stat.m_uncomp_size) {
            throw LibraryError("Archive exceeds cartridge extraction limits");
        }
        total_size += stat.m_uncomp_size;
    }
    return entries;
}

void extract_archive(const fs::path &archive, const fs::path &destination) {
    ZipReader reader(archive);
    for (const auto &entry : checked_entries(reader, archive)) {
        const fs::path target = destination / fs::path(entry.path).lexically_normal();
        if (entry.is_directory) {
            fs::create_directories(target);
            continue;
        }
        fs::create_directories(target.parent_path());
        if (!mz_zip_reader_extract_to_file(reader.get(), entry.index, target.string().c_str(), 0)) {
            throw LibraryError("Cannot extract archive entry: " + entry.path);
        }
    }
}

// Where the package is in an extracted archive: at the top, or in the one directory
// that holds a manifest. Archives made on macOS carry a second directory beside it.
fs::path extracted_package(const fs::path &root) {
    if (fs::exists(root / "scenario.json")) {
        return root;
    }
    std::vector<fs::path> candidates;
    for (const auto &entry : fs::directory_iterator(root)) {
        if (entry.is_directory() && fs::exists(entry.path() / "scenario.json")) {
            candidates.push_back(entry.path());
        }
    }
    if (candidates.size() != 1) {
        throw LibraryError("Archive does not contain one cartridge");
    }
    return candidates.front();
}

bool is_archive_name(const fs::path &path) {
    const std::string extension = portable_archive_key(path.extension().string());
    return extension == ".zip" || extension == ".chronicle";
}

void require_valid(const fs::path &package) {
    const auto issues = validate_package(package);
    if (!has_errors(issues)) {
        return;
    }
    std::string message = "Cartridge failed validation:";
    for (const auto &issue : issues) {
        if (issue.level == IssueLevel::error) {
            message += "\n" + issue.to_string();
        }
    }
    throw LibraryError(message);
}

void copy_package(const fs::path &source, const fs::path &destination) {
    const auto contents = inspect_package_tree(source);
    for (const auto &file : contents.files) {
        const fs::path relative = fs::relative(file, source);
        const fs::path target = destination / relative;
        fs::create_directories(target.parent_path());
        fs::copy_file(file, target, fs::copy_options::none);
    }
}

bool path_is_within(const fs::path &candidate, const fs::path &root) {
    const fs::path relative = candidate.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
}

// Swap two directories in one step, so no moment exists without a cartridge at the
// destination. False when the file system cannot.
bool exchange(const fs::path &left, const fs::path &right) {
#if defined(__linux__)
    return ::renameat2(AT_FDCWD, left.c_str(), AT_FDCWD, right.c_str(), RENAME_EXCHANGE) == 0;
#elif defined(__APPLE__)
    return ::renamex_np(left.c_str(), right.c_str(), RENAME_SWAP) == 0;
#else
    (void)left;
    (void)right;
    return false;
#endif
}

void commit_replacement(ScopedDirectory &staged, const fs::path &destination) {
    const bool destination_exists =
        fs::symlink_status(destination).type() != fs::file_type::not_found;
    if (destination_exists && exchange(staged.path(), destination)) {
        return; // The staging directory now holds the old version, and removes it.
    }
    const fs::path backup =
        staged.path().parent_path() /
        (".backup-" + destination.filename().string() + "-" + staged.path().filename().string());
    if (destination_exists) {
        fs::rename(destination, backup);
    }
    try {
        fs::rename(staged.path(), destination);
        staged.release();
    } catch (...) {
        if (destination_exists) {
            std::error_code ignored;
            fs::rename(backup, destination, ignored);
        }
        throw;
    }
    if (destination_exists) {
        std::error_code ec;
        fs::remove_all(backup, ec);
        if (ec) {
            throw LibraryError("Installed cartridge, but could not remove its backup: " +
                               ec.message());
        }
    }
}

} // namespace

fs::path default_library_dir() {
    return chronicle_home() / "cartridges";
}

std::vector<CartridgeInfo> list_cartridges(const std::optional<fs::path> &library_dir) {
    const fs::path root = library_dir.value_or(default_library_dir());
    std::vector<CartridgeInfo> results;
    if (!fs::exists(root)) {
        return results;
    }
    std::vector<fs::path> children;
    for (const auto &entry : fs::directory_iterator(root)) {
        if (!entry.is_symlink() && entry.is_directory() &&
            !entry.path().filename().string().starts_with('.')) {
            children.push_back(entry.path());
        }
    }
    std::ranges::sort(children);
    for (const auto &path : children) {
        try {
            if (!fs::exists(path / "scenario.json")) {
                continue;
            }
            const auto manifest = load_manifest(path);
            if (!is_safe_cartridge_id(manifest.id) || manifest.id != path.filename()) {
                continue;
            }
            results.push_back({.id = manifest.id,
                               .name = manifest.name,
                               .version = manifest.version,
                               .path = path.string()});
        } catch (const std::exception &) {
            continue;
        }
    }
    return results;
}

fs::path install_cartridge(const fs::path &source, const std::optional<fs::path> &library_dir) {
    const fs::path src = fs::weakly_canonical(source);
    const fs::path requested_root = library_dir.value_or(default_library_dir());
    fs::create_directories(requested_root);
    const fs::path root = fs::weakly_canonical(requested_root);

    std::optional<ScopedDirectory> extraction;
    fs::path package;
    if (fs::is_regular_file(src) && is_archive_name(src)) {
        extraction.emplace(create_unique_directory(root, ".extract-"));
        extract_archive(src, extraction->path());
        package = extracted_package(extraction->path());
    } else if (fs::is_directory(src)) {
        package = src;
    } else {
        throw LibraryError("Cannot install: " + src.string());
    }

    require_valid(package);
    const auto manifest = load_manifest(package);
    if (!is_safe_cartridge_id(manifest.id)) {
        throw LibraryError("Unsafe cartridge id: " + manifest.id);
    }

    ScopedDirectory staged(create_unique_directory(root, ".install-" + manifest.id + "-"));
    copy_package(package, staged.path());
    require_valid(staged.path());

    const fs::path destination = root / manifest.id;
    commit_replacement(staged, destination);
    return destination;
}

fs::path pack_cartridge(const fs::path &package_dir, const fs::path &output) {
    const fs::path src = fs::weakly_canonical(package_dir);
    if (!is_archive_name(output)) {
        throw LibraryError("Archive output must end in .chronicle or .zip: " + output.string());
    }
    const fs::path absolute_output = fs::absolute(output).lexically_normal();
    fs::create_directories(absolute_output.parent_path());
    const fs::path resolved_output =
        fs::weakly_canonical(absolute_output.parent_path()) / absolute_output.filename();
    if (path_is_within(resolved_output, src)) {
        throw LibraryError("Archive output must be outside the source package");
    }

    require_valid(src);
    const auto contents = inspect_package_tree(src);
    ScopedDirectory temporary(create_unique_directory(resolved_output.parent_path(), ".pack-"));
    const fs::path temporary_archive = temporary.path() / resolved_output.filename();

    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, temporary_archive.string().c_str(), 0)) {
        throw LibraryError("Cannot create archive: " + resolved_output.string());
    }
    for (const auto &file : contents.files) {
        const std::string archive_name = fs::relative(file, src).generic_string();
        if (!mz_zip_writer_add_file(&zip, archive_name.c_str(), file.string().c_str(), nullptr, 0,
                                    MZ_DEFAULT_LEVEL)) {
            mz_zip_writer_end(&zip);
            throw LibraryError("Cannot add file to archive: " + archive_name);
        }
    }
    if (!mz_zip_writer_finalize_archive(&zip)) {
        mz_zip_writer_end(&zip);
        throw LibraryError("Cannot finalize archive: " + resolved_output.string());
    }
    if (!mz_zip_writer_end(&zip)) {
        throw LibraryError("Cannot close archive: " + resolved_output.string());
    }

    {
        // Never hand out an archive that install would turn away.
        ZipReader reader(temporary_archive);
        (void)checked_entries(reader, temporary_archive);
    }
    if (fs::is_directory(resolved_output)) {
        throw LibraryError("Archive output is a directory: " + resolved_output.string());
    }
    fs::rename(temporary_archive, resolved_output);
    return resolved_output;
}

fs::path resolve_scenario(const std::optional<std::string> &scenario,
                          const std::optional<fs::path> &library_dir) {
    if (scenario && !scenario->empty()) {
        const fs::path path(*scenario);
        if (fs::is_directory(path)) {
            return fs::weakly_canonical(path);
        }
        if (!is_safe_cartridge_id(*scenario)) {
            throw LibraryError("Invalid cartridge id: " + *scenario);
        }
        const fs::path root = library_dir.value_or(default_library_dir());
        const fs::path candidate = root / *scenario;
        if (fs::is_directory(candidate) && !fs::is_symlink(candidate)) {
            return fs::weakly_canonical(candidate);
        }
        throw LibraryError("Scenario not found: " + *scenario);
    }
    if (fs::is_directory("examples/minimal")) {
        return fs::weakly_canonical("examples/minimal");
    }
    if (const auto installed = installed_minimal_example()) {
        return *installed;
    }
    throw LibraryError("No scenario specified and no examples/minimal found");
}

InspectInfo inspect_package(const fs::path &package_dir) {
    const auto manifest = load_manifest(package_dir);
    const auto issues = validate_package(package_dir);
    InspectInfo info{.id = manifest.id,
                     .name = manifest.name,
                     .version = manifest.version,
                     .schema = manifest.chronicle_schema_version,
                     .path = package_dir.string(),
                     .ready = true,
                     .errors = {},
                     .warnings = {}};
    for (const auto &issue : issues) {
        if (issue.level == IssueLevel::error) {
            info.errors.push_back(issue.to_string());
        } else {
            info.warnings.push_back(issue.to_string());
        }
    }
    info.ready = info.errors.empty();
    return info;
}

} // namespace chronicle
