#include "chronicle/cartridge/loader.hpp"

#include <fstream>
#include <sstream>

namespace chronicle {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

json read_json_file(const fs::path &path) {
    std::error_code ec;
    const auto status = fs::status(path, ec);
    if (ec || !fs::is_regular_file(status)) {
        throw CartridgeError("Missing package file: " + path.string());
    }
    const auto size = fs::file_size(path, ec);
    if (ec || size > MAX_PACKAGE_FILE_BYTES) {
        throw CartridgeError("Package file exceeds the 4 MiB limit: " + path.string());
    }
    std::ifstream stream(path);
    if (!stream) {
        throw CartridgeError("Missing package file: " + path.string());
    }
    try {
        return json::parse(stream);
    } catch (const json::parse_error &exc) {
        throw CartridgeError("Invalid JSON in " + path.string() + ": " + exc.what());
    }
}

// Reject absolute paths, parent traversal, and anything resolving outside root.
fs::path resolve_inside(const fs::path &root, const std::string &relative) {
    const fs::path rel(relative);
    if (relative.empty() || relative.contains('\\') || rel.is_absolute() || rel.has_root_name() ||
        rel.has_root_directory()) {
        throw CartridgeError("Unsafe package path: '" + relative + "'");
    }
    for (const auto &part : rel) {
        if (part == "." || part == ".." || part.empty()) {
            throw CartridgeError("Unsafe package path: '" + relative + "'");
        }
    }
    const fs::path resolved = fs::weakly_canonical(root / rel);
    const fs::path root_resolved = fs::weakly_canonical(root);
    const fs::path from_root = resolved.lexically_relative(root_resolved);
    if (from_root.empty() || from_root.is_absolute() ||
        (!from_root.empty() && *from_root.begin() == "..")) {
        throw CartridgeError("Path escapes package directory: '" + relative + "'");
    }
    return resolved;
}

template <typename T> T parse_as(const json &raw, const char *what) {
    try {
        return raw.get<T>();
    } catch (const std::exception &exc) {
        throw CartridgeError(std::string(what) + ": " + exc.what());
    }
}

} // namespace

PackageContents inspect_package_tree(const fs::path &package_dir) {
    const fs::path root = fs::weakly_canonical(package_dir);
    if (!fs::is_directory(root)) {
        throw CartridgeError("Not a directory: " + root.string());
    }

    PackageContents contents;
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::none, ec);
    if (ec) {
        throw CartridgeError("Cannot inspect package: " + ec.message());
    }
    for (const fs::recursive_directory_iterator end; it != end; it.increment(ec)) {
        if (ec) {
            throw CartridgeError("Cannot inspect package: " + ec.message());
        }
        const auto link_status = it->symlink_status(ec);
        if (ec) {
            throw CartridgeError("Cannot inspect package entry: " + it->path().string());
        }
        if (fs::is_symlink(link_status)) {
            throw CartridgeError("Package contains a symbolic link: " + it->path().string());
        }
        if (fs::is_directory(link_status)) {
            continue;
        }
        if (!fs::is_regular_file(link_status)) {
            throw CartridgeError("Package contains a special file: " + it->path().string());
        }
        if (contents.files.size() >= MAX_PACKAGE_FILES) {
            throw CartridgeError("Package contains more than 256 files");
        }
        const auto size = it->file_size(ec);
        if (ec || size > MAX_PACKAGE_FILE_BYTES) {
            throw CartridgeError("Package file exceeds the 4 MiB limit: " + it->path().string());
        }
        if (contents.total_bytes > MAX_PACKAGE_TOTAL_BYTES - size) {
            throw CartridgeError("Package exceeds the 32 MiB total size limit");
        }
        contents.total_bytes += size;
        contents.files.push_back(it->path());
    }
    if (ec) {
        throw CartridgeError("Cannot inspect package: " + ec.message());
    }
    std::ranges::sort(contents.files);
    return contents;
}

ScenarioManifest load_manifest(const fs::path &package_dir) {
    const fs::path root = fs::weakly_canonical(package_dir);
    const json raw = read_json_file(resolve_inside(root, "scenario.json"));
    return parse_as<ScenarioManifest>(raw, "scenario.json");
}

WorldState assemble_world(const json &manifest_raw, const json &config_raw, const json &world_raw,
                          const json &npcs_raw, const json &facts_raw, const json &flags_raw,
                          const json &events_raw) {
    WorldState state;
    state.manifest = parse_as<ScenarioManifest>(manifest_raw, "scenario.json");
    state.config = config_raw.is_null() || config_raw.empty()
                       ? parse_as<ConfigData>(json::object(), "config")
                       : parse_as<ConfigData>(config_raw, "config");
    const auto world = parse_as<WorldData>(world_raw, "world");
    auto npcs_file = parse_as<NpcsFile>(npcs_raw, "npcs");
    const auto facts_file = parse_as<FactsFile>(facts_raw, "facts");
    const auto flags_file = parse_as<FlagsFile>(flags_raw, "flags");
    const auto events_file = parse_as<EventsFile>(events_raw, "events");

    // Ensure npc identity ids match map keys.
    for (auto &[npc_id, npc] : npcs_file.npcs) {
        if (npc.identity.id.empty()) {
            npc.identity.id = npc_id;
        }
    }

    state.locations = world.locations;
    state.items = world.items;
    state.npcs = npcs_file.npcs;
    state.facts = facts_file.facts;
    state.flag_meta = flags_file.flags;
    for (const auto &[flag_id, meta] : flags_file.flags) {
        state.flags[flag_id] = meta.default_value;
    }
    state.events = events_file.events;

    for (const auto &[loc_id, loc] : world.locations) {
        for (const auto &item_id : loc.items) {
            state.item_owners[item_id] = "location";
            state.item_locations[item_id] = loc_id;
        }
    }
    for (const auto &[npc_id, npc] : state.npcs) {
        for (const auto &item_id : npc.state.inventory) {
            state.item_owners[item_id] = npc_id;
            state.item_locations[item_id] = npc.state.current_location;
        }
    }
    for (const auto &[fact_id, fact] : state.facts) {
        if (fact.revealed_by_default) {
            state.revealed_facts.insert(fact_id);
        }
    }

    state.player = PlayerState{.current_location = world.start_location, .inventory = {}};
    state.clock = ClockState{.turns_elapsed = 0,
                             .turns_per_period = state.config.turns_per_period,
                             .total_periods = state.config.total_periods};
    return state;
}

WorldState load_package(const fs::path &package_dir) {
    const fs::path root = fs::weakly_canonical(package_dir);
    if (!fs::is_directory(root)) {
        throw CartridgeError("Not a directory: " + root.string());
    }

    (void)inspect_package_tree(root);

    const json manifest_raw = read_json_file(resolve_inside(root, "scenario.json"));
    const auto manifest = parse_as<ScenarioManifest>(manifest_raw, "scenario.json");
    const auto &files = manifest.files;

    const json config_raw = read_json_file(resolve_inside(root, files.config));
    const json world_raw = read_json_file(resolve_inside(root, files.world));
    const json npcs_raw = read_json_file(resolve_inside(root, files.npcs));
    const json facts_raw = read_json_file(resolve_inside(root, files.facts));
    const json flags_raw = read_json_file(resolve_inside(root, files.flags));
    const json events_raw = read_json_file(resolve_inside(root, files.events));

    return assemble_world(manifest_raw, config_raw, world_raw, npcs_raw, facts_raw, flags_raw,
                          events_raw);
}

} // namespace chronicle
