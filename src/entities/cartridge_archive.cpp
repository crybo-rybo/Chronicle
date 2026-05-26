/**
 * @file cartridge_archive.cpp
 * @brief Implementation of cartridge archive pack/install helpers.
 */

#include "entities/cartridge_archive.hpp"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace chronicle {

namespace {

constexpr const char *kArchiveSuffix = ".chronicle";

bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

void validate_path_segment(std::string_view value, std::string_view label) {
    if (value.empty() || value == "." || value == ".." ||
        value.find('/') != std::string_view::npos || value.find('\\') != std::string_view::npos) {
        throw std::runtime_error(std::string(label) +
                                 " must be a safe single path segment: " + std::string(value));
    }
}

int run_process(const std::vector<std::string> &args, std::string *stdout_capture = nullptr) {
    if (args.empty()) {
        throw std::invalid_argument("run_process requires argv");
    }

    int pipefd[2] = {-1, -1};
    if (stdout_capture && pipe(pipefd) != 0) {
        throw std::runtime_error("Failed to create process pipe: " +
                                 std::string(std::strerror(errno)));
    }

    const pid_t pid = fork();
    if (pid < 0) {
        if (pipefd[0] != -1) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        throw std::runtime_error("Failed to fork process: " + std::string(std::strerror(errno)));
    }

    if (pid == 0) {
        if (stdout_capture) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }

        const int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null >= 0) {
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }

        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (const auto &arg : args) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    if (stdout_capture) {
        close(pipefd[1]);
        stdout_capture->clear();
        char buffer[4096];
        while (true) {
            const ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer));
            if (bytes_read > 0) {
                stdout_capture->append(buffer, static_cast<std::size_t>(bytes_read));
                continue;
            }
            if (bytes_read == -1 && errno == EINTR) {
                continue;
            }
            break;
        }
        close(pipefd[0]);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            throw std::runtime_error("Failed to wait for process: " +
                                     std::string(std::strerror(errno)));
        }
    }

    if (!WIFEXITED(status)) {
        return 1;
    }
    return WEXITSTATUS(status);
}

void run_tar_or_throw(const std::vector<std::string> &args, const std::string &message) {
    if (run_process(args) != 0) {
        throw std::runtime_error(message);
    }
}

bool archive_entry_is_safe(std::string_view entry) {
    if (entry.empty()) {
        return false;
    }

    std::filesystem::path path(entry);
    if (path.is_absolute()) {
        return false;
    }

    for (const auto &part : path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

void validate_archive_entries(const std::filesystem::path &archive_path) {
    std::string listing;
    const auto archive = std::filesystem::absolute(archive_path).string();
    if (run_process({"tar", "-tzf", archive}, &listing) != 0) {
        throw std::runtime_error("Failed to read cartridge archive: " + archive_path.string());
    }

    std::istringstream lines(listing);
    std::string entry;
    while (std::getline(lines, entry)) {
        if (!archive_entry_is_safe(entry)) {
            throw std::runtime_error("Unsafe path in cartridge archive: " + entry);
        }
    }

    std::string verbose_listing;
    if (run_process({"tar", "-tvzf", archive}, &verbose_listing) != 0) {
        throw std::runtime_error("Failed to inspect cartridge archive: " + archive_path.string());
    }

    std::istringstream verbose_lines(verbose_listing);
    std::string verbose_entry;
    while (std::getline(verbose_lines, verbose_entry)) {
        if (!verbose_entry.empty() &&
            (verbose_entry.front() == 'l' || verbose_entry.front() == 'h')) {
            throw std::runtime_error("Cartridge archive must not contain links: " +
                                     archive_path.string());
        }
    }
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

std::filesystem::path install_dir_for_id(const std::filesystem::path &library_root,
                                         const std::string &cartridge_id) {
    validate_path_segment(cartridge_id, "Cartridge id");
    return library_root / cartridge_id;
}

class TemporaryDirectory {
  public:
    explicit TemporaryDirectory(std::filesystem::path parent) {
        std::filesystem::create_directories(parent);
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int index = 0; index < 100; ++index) {
            path_ = parent / (".chronicle_tmp_" + std::to_string(getpid()) + "_" +
                              std::to_string(stamp) + "_" + std::to_string(index));
            std::error_code ec;
            if (std::filesystem::create_directory(path_, ec)) {
                return;
            }
        }
        throw std::runtime_error("Failed to create temporary cartridge directory under " +
                                 parent.string());
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path &path() const noexcept { return path_; }

  private:
    std::filesystem::path path_;
};

void copy_file_preserving_manifest_path(const std::filesystem::path &source,
                                        const std::filesystem::path &destination_root,
                                        const std::string &manifest_path) {
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(source)) ||
        !std::filesystem::is_regular_file(source)) {
        throw std::runtime_error("Package file must be a regular file inside the cartridge: " +
                                 source.string());
    }
    const auto destination = destination_root / std::filesystem::path(manifest_path);
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy_file(source, destination,
                               std::filesystem::copy_options::overwrite_existing);
}

void copy_package_files(const ScenarioPackage &package,
                        const std::filesystem::path &destination_root) {
    std::filesystem::create_directories(destination_root);
    copy_file_preserving_manifest_path(package.root_dir / "scenario.json", destination_root,
                                       "scenario.json");

    copy_file_preserving_manifest_path(package.config_path, destination_root,
                                       package.manifest.files.config);
    copy_file_preserving_manifest_path(package.world_files.world, destination_root,
                                       package.manifest.files.world);
    copy_file_preserving_manifest_path(package.world_files.npcs, destination_root,
                                       package.manifest.files.npcs);
    copy_file_preserving_manifest_path(package.world_files.facts, destination_root,
                                       package.manifest.files.facts);
    copy_file_preserving_manifest_path(package.world_files.flags, destination_root,
                                       package.manifest.files.flags);
    copy_file_preserving_manifest_path(package.world_files.events, destination_root,
                                       package.manifest.files.events);
}

void extract_archive_to(const std::filesystem::path &archive_path,
                        const std::filesystem::path &destination) {
    validate_archive_entries(archive_path);
    std::filesystem::create_directories(destination);
    const auto archive = std::filesystem::absolute(archive_path).string();
    const auto dest = std::filesystem::absolute(destination).string();
    run_tar_or_throw({"tar", "-xzf", archive, "-C", dest},
                     "Failed to extract cartridge archive: " + archive);
}

void throw_if_installing_directory_into_itself(const std::filesystem::path &source,
                                               const std::filesystem::path &installed_dir) {
    std::error_code ec;
    if (std::filesystem::exists(source, ec) && std::filesystem::exists(installed_dir, ec) &&
        std::filesystem::equivalent(source, installed_dir, ec)) {
        throw std::runtime_error("Cartridge is already installed at target path: " +
                                 installed_dir.string());
    }
}

} // namespace

std::filesystem::path pack_cartridge(const std::filesystem::path &scenario_dir,
                                     const std::filesystem::path &output_path) {
    validate_or_throw(scenario_dir);

    auto package = load_scenario_package(scenario_dir);
    validate_path_segment(package.manifest.id, "Cartridge id");
    std::filesystem::path archive_path = output_path;
    if (archive_path.empty()) {
        archive_path = package.manifest.id + kArchiveSuffix;
    } else if (archive_path.extension().empty()) {
        archive_path += kArchiveSuffix;
    }

    if (archive_path.has_parent_path()) {
        std::filesystem::create_directories(archive_path.parent_path());
    }

    TemporaryDirectory staging(std::filesystem::temp_directory_path());
    copy_package_files(package, staging.path());

    const auto destination = std::filesystem::absolute(archive_path).string();
    run_tar_or_throw(
        {"tar", "-czf", destination, "-C", std::filesystem::absolute(staging.path()).string(), "."},
        "Failed to create cartridge archive: " + destination);
    return archive_path;
}

ScenarioPackage install_cartridge(const std::filesystem::path &source,
                                  const std::filesystem::path &library_root) {
    const auto resolved_library = library_root.empty() ? default_library_root() : library_root;
    std::filesystem::create_directories(resolved_library);

    std::filesystem::path installed_dir;
    if (ends_with(source.filename().string(), kArchiveSuffix)) {
        TemporaryDirectory extracted(resolved_library);
        extract_archive_to(source, extracted.path());
        validate_or_throw(extracted.path());
        auto package = load_scenario_package(extracted.path());
        installed_dir = install_dir_for_id(resolved_library, package.manifest.id);
        if (std::filesystem::exists(installed_dir)) {
            std::filesystem::remove_all(installed_dir);
        }
        copy_package_files(package, installed_dir);
    } else {
        validate_or_throw(source);
        auto package = load_scenario_package(source);
        installed_dir = install_dir_for_id(resolved_library, package.manifest.id);
        throw_if_installing_directory_into_itself(source, installed_dir);
        if (std::filesystem::exists(installed_dir)) {
            std::filesystem::remove_all(installed_dir);
        }
        copy_package_files(package, installed_dir);
    }

    validate_or_throw(installed_dir);
    return load_scenario_package(installed_dir);
}

} // namespace chronicle
