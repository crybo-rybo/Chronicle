#include "chronicle/library.hpp"

#include <cstdlib>
#include <miniz.h>

#include "chronicle/cartridge/loader.hpp"

namespace chronicle {

namespace fs = std::filesystem;

namespace {

// Reject absolute entries and parent traversal (zip-slip).
bool safe_archive_entry(const std::string &name) {
    if (name.empty() || name.front() == '/' || name.front() == '\\') {
        return false;
    }
    const fs::path entry(name);
    for (const auto &part : entry) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

void extract_archive(const fs::path &archive, const fs::path &destination) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archive.string().c_str(), 0)) {
        throw LibraryError("Cannot open archive: " + archive.string());
    }
    const auto count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint index = 0; index < count; ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, index, &stat)) {
            mz_zip_reader_end(&zip);
            throw LibraryError("Cannot read archive entry in " + archive.string());
        }
        const std::string name = stat.m_filename;
        if (!safe_archive_entry(name)) {
            mz_zip_reader_end(&zip);
            throw LibraryError("Unsafe archive entry: " + name);
        }
        const fs::path target = destination / name;
        if (mz_zip_reader_is_file_a_directory(&zip, index)) {
            fs::create_directories(target);
            continue;
        }
        fs::create_directories(target.parent_path());
        if (!mz_zip_reader_extract_to_file(&zip, index, target.string().c_str(), 0)) {
            mz_zip_reader_end(&zip);
            throw LibraryError("Cannot extract archive entry: " + name);
        }
    }
    mz_zip_reader_end(&zip);
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

} // namespace

fs::path default_library_dir() {
    const char *home = std::getenv("HOME");
    const fs::path base = home != nullptr ? fs::path(home) : fs::current_path();
    return base / ".chronicle" / "cartridges";
}

std::vector<CartridgeInfo> list_cartridges(const std::optional<fs::path> &library_dir) {
    const fs::path root = library_dir.value_or(default_library_dir());
    std::vector<CartridgeInfo> results;
    if (!fs::exists(root)) {
        return results;
    }
    std::vector<fs::path> children;
    for (const auto &entry : fs::directory_iterator(root)) {
        if (entry.is_directory()) {
            children.push_back(entry.path());
        }
    }
    std::sort(children.begin(), children.end());
    for (const auto &path : children) {
        if (!fs::exists(path / "scenario.json")) {
            continue;
        }
        try {
            const auto manifest = load_manifest(path);
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
    const fs::path src = fs::absolute(source);
    const fs::path root = library_dir.value_or(default_library_dir());
    fs::create_directories(root);

    fs::path package;
    fs::path staging;
    const bool is_archive =
        fs::is_regular_file(src) && (src.extension() == ".zip" || src.extension() == ".chronicle");
    if (is_archive) {
        staging = root / (".staging_" + src.stem().string());
        fs::remove_all(staging);
        fs::create_directories(staging);
        extract_archive(src, staging);
        // If the archive contained a single top-level dir, use that.
        std::vector<fs::path> children;
        for (const auto &entry : fs::directory_iterator(staging)) {
            if (!entry.path().filename().string().starts_with(".")) {
                children.push_back(entry.path());
            }
        }
        package =
            children.size() == 1 && fs::is_directory(children.front()) ? children.front() : staging;
    } else if (fs::is_directory(src)) {
        package = src;
    } else {
        throw LibraryError("Cannot install: " + src.string());
    }

    try {
        require_valid(package);
        const auto manifest = load_manifest(package);
        const fs::path dest = root / manifest.id;
        fs::remove_all(dest);
        fs::copy(package, dest, fs::copy_options::recursive);
        if (!staging.empty()) {
            fs::remove_all(staging);
        }
        return dest;
    } catch (...) {
        if (!staging.empty()) {
            fs::remove_all(staging);
        }
        throw;
    }
}

fs::path pack_cartridge(const fs::path &package_dir, const fs::path &output) {
    const fs::path src = fs::absolute(package_dir);
    require_valid(src);

    if (output.has_parent_path()) {
        fs::create_directories(output.parent_path());
    }
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, output.string().c_str(), 0)) {
        throw LibraryError("Cannot create archive: " + output.string());
    }
    std::vector<fs::path> files;
    for (const auto &entry : fs::recursive_directory_iterator(src)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto &file : files) {
        const std::string arcname = fs::relative(file, src).generic_string();
        if (!mz_zip_writer_add_file(&zip, arcname.c_str(), file.string().c_str(), nullptr, 0,
                                    MZ_DEFAULT_LEVEL)) {
            mz_zip_writer_end(&zip);
            throw LibraryError("Cannot add file to archive: " + arcname);
        }
    }
    if (!mz_zip_writer_finalize_archive(&zip)) {
        mz_zip_writer_end(&zip);
        throw LibraryError("Cannot finalize archive: " + output.string());
    }
    mz_zip_writer_end(&zip);
    return output;
}

fs::path resolve_scenario(const std::optional<std::string> &scenario,
                          const std::optional<fs::path> &library_dir) {
    if (scenario && !scenario->empty()) {
        const fs::path path(*scenario);
        if (fs::is_directory(path)) {
            return fs::absolute(path);
        }
        // Treat as a library id.
        const fs::path root = library_dir.value_or(default_library_dir());
        const fs::path candidate = root / *scenario;
        if (fs::is_directory(candidate)) {
            return fs::absolute(candidate);
        }
        throw LibraryError("Scenario not found: " + *scenario);
    }
    for (const auto *candidate : {"examples/minimal", "examples/minimal_scenario"}) {
        if (fs::is_directory(candidate)) {
            return fs::absolute(candidate);
        }
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
