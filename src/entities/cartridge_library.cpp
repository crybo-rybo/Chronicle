/**
 * @file cartridge_library.cpp
 * @brief Implementation of cartridge library discovery.
 */

#include "entities/cartridge_library.hpp"
#include <cstdlib>
#include <unordered_map>

namespace chronicle {

namespace {

#if defined(_WIN32)
constexpr char kPathSeparator = ';';
#else
constexpr char kPathSeparator = ':';
#endif

bool is_regular_directory(const std::filesystem::path &path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

std::optional<CartridgeEntry> try_load_entry(const std::filesystem::path &root) {
    const auto manifest_path = root / "scenario.json";
    if (!std::filesystem::is_regular_file(manifest_path)) {
        return std::nullopt;
    }

    try {
        ScenarioManifest manifest = load_scenario_manifest(root);
        CartridgeEntry entry;
        entry.id = manifest.id;
        entry.name = manifest.name;
        entry.version = manifest.version;
        entry.chronicle_schema_version = manifest.chronicle_schema_version;
        entry.root_dir = std::filesystem::weakly_canonical(root);
        entry.metadata = manifest.metadata;
        return entry;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

void append_unique_root(std::vector<std::filesystem::path> &roots,
                        const std::filesystem::path &candidate) {
    if (!is_regular_directory(candidate)) {
        return;
    }
    const auto canonical = std::filesystem::weakly_canonical(candidate);
    if (std::ranges::find(roots, canonical) == roots.end()) {
        roots.push_back(canonical);
    }
}

void append_env_roots(std::vector<std::filesystem::path> &roots) {
    const char *library_path = std::getenv("CHRONICLE_LIBRARY_PATH");
    if (!library_path || *library_path == '\0') {
        return;
    }

    std::string value(library_path);
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(kPathSeparator, start);
        const auto token =
            value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!token.empty()) {
            append_unique_root(roots, token);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

} // namespace

std::vector<std::filesystem::path> default_cartridge_library_roots() {
    std::vector<std::filesystem::path> roots;
    append_env_roots(roots);

    if (const char *home = std::getenv("HOME")) {
        append_unique_root(roots, std::filesystem::path(home) / ".chronicle" / "cartridges");
    }

    append_unique_root(roots, "cartridges");
    return roots;
}

std::vector<CartridgeEntry> list_cartridges(const std::vector<std::filesystem::path> &extra_roots) {
    std::vector<std::filesystem::path> roots;
    for (const auto &root : extra_roots) {
        append_unique_root(roots, root);
    }
    for (const auto &root : default_cartridge_library_roots()) {
        append_unique_root(roots, root);
    }

    std::unordered_map<std::string, CartridgeEntry> by_id;
    for (const auto &library_root : roots) {
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(library_root, ec)) {
            if (ec || !entry.is_directory()) {
                continue;
            }
            if (auto cartridge = try_load_entry(entry.path())) {
                by_id.try_emplace(cartridge->id, std::move(*cartridge));
            }
        }
    }

    std::vector<CartridgeEntry> cartridges;
    cartridges.reserve(by_id.size());
    for (auto &[id, cartridge] : by_id) {
        cartridges.push_back(std::move(cartridge));
    }

    std::ranges::sort(cartridges, {}, &CartridgeEntry::id);
    return cartridges;
}

std::optional<CartridgeEntry>
find_cartridge(std::string_view cartridge_id,
               const std::vector<std::filesystem::path> &extra_roots) {
    for (const auto &entry : list_cartridges(extra_roots)) {
        if (entry.id == cartridge_id) {
            return entry;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path>
resolve_cartridge_path(std::string_view scenario_ref,
                       const std::vector<std::filesystem::path> &extra_roots) {
    const std::filesystem::path candidate(scenario_ref);
    std::error_code ec;
    if (is_regular_directory(candidate)) {
        return std::filesystem::weakly_canonical(candidate, ec);
    }

    if (auto entry = find_cartridge(scenario_ref, extra_roots)) {
        return entry->root_dir;
    }

    return std::nullopt;
}

} // namespace chronicle
