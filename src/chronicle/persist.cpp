#include "chronicle/persist.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "chronicle/cartridge/validator.hpp"

namespace chronicle {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

std::atomic_uint64_t temp_sequence{0};

SaveError error(const SaveError::Kind kind, std::string message) {
    return {.kind = kind, .message = std::move(message)};
}

bool valid_slot(const int slot) {
    return slot >= MIN_SAVE_SLOT && slot <= MAX_SAVE_SLOT;
}

std::optional<std::string> exact_fields_issue(const json &document,
                                              const std::set<std::string> &expected,
                                              const std::string_view context) {
    if (!document.is_object()) {
        return std::string(context) + " must be an object";
    }
    for (const auto &field : expected) {
        if (!document.contains(field)) {
            return std::string(context) + " is missing '" + field + "'";
        }
    }
    for (const auto &entry : document.items()) {
        if (!expected.contains(entry.key())) {
            return std::string(context) + " has unknown field '" + entry.key() + "'";
        }
    }
    return std::nullopt;
}

template <typename Left, typename Right>
bool same_keys(const std::map<std::string, Left> &left, const std::map<std::string, Right> &right) {
    return left.size() == right.size() &&
           std::ranges::equal(
               left, right, {}, [](const auto &entry) { return entry.first; },
               [](const auto &entry) { return entry.first; });
}

json authored_definition(const WorldState &world) {
    json locations = json::object();
    for (const auto &[location_id, location] : world.locations) {
        locations[location_id] = {
            {"name", location.name},
            {"base_description", location.base_description},
            {"exits", location.exits},
        };
    }

    json npc_identities = json::object();
    for (const auto &[npc_id, npc] : world.npcs) {
        npc_identities[npc_id] = npc.identity;
    }

    json events = json::object();
    for (const auto &[event_id, event] : world.events) {
        auto definition = event;
        definition.fired = false;
        events[event_id] = definition;
    }

    return {
        {"manifest", world.manifest},
        {"config", world.config},
        {"locations", std::move(locations)},
        {"items", world.items},
        {"npc_identities", std::move(npc_identities)},
        {"facts", world.facts},
        {"flag_meta", world.flag_meta},
        {"events", std::move(events)},
    };
}

bool same_locked_entry(const LockedExitEntry &left, const LockedExitEntry &right) {
    return left.direction == right.direction && left.unlocked == right.unlocked;
}

std::optional<std::string> locked_exit_issue(const WorldState &canonical,
                                             const WorldState &candidate) {
    for (const auto &[location_id, location] : candidate.locations) {
        const auto &authored = canonical.locations.at(location_id).locked_exits;
        std::set<std::string> directions;
        for (const auto &entry : location.locked_exits) {
            if (!directions.insert(entry.direction).second) {
                return "location '" + location_id + "' has duplicate locked exit '" +
                       entry.direction + "'";
            }
            if (std::ranges::find_if(authored, [&](const LockedExitEntry &original) {
                    return same_locked_entry(entry, original);
                }) == authored.end()) {
                return "location '" + location_id + "' has a locked exit not in the cartridge";
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> runtime_state_issue(const WorldState &canonical,
                                               const WorldState &candidate) {
    if (authored_definition(canonical) != authored_definition(candidate)) {
        return "world contains authored data that differs from the cartridge";
    }
    if (!same_keys(canonical.flags, candidate.flags)) {
        return "world flag set differs from the cartridge";
    }
    if (const auto issue = locked_exit_issue(canonical, candidate)) {
        return issue;
    }
    const auto issues = validate_world(candidate);
    const auto invalid = std::ranges::find(issues, IssueLevel::error, &ValidationIssue::level);
    if (invalid != issues.end()) {
        return "world failed validation: " + invalid->message;
    }
    return std::nullopt;
}

json runtime_state_document(const WorldState &world) {
    json npc_states = json::object();
    for (const auto &[npc_id, npc] : world.npcs) {
        npc_states[npc_id] = npc.state;
    }

    json event_fired = json::object();
    for (const auto &[event_id, event] : world.events) {
        event_fired[event_id] = event.fired;
    }

    json locked_exits = json::object();
    for (const auto &[location_id, location] : world.locations) {
        locked_exits[location_id] = location.locked_exits;
    }

    return {
        {"player", world.player},
        {"clock", world.clock},
        {"npc_states", std::move(npc_states)},
        {"flags", world.flags},
        {"event_fired", std::move(event_fired)},
        {"revealed_facts", world.revealed_facts},
        {"item_positions", world.item_positions},
        {"locked_exits", std::move(locked_exits)},
    };
}

std::expected<WorldState, std::string> restore_runtime_state(const WorldState &canonical,
                                                             const json &document) {
    static const std::set<std::string> fields = {
        "clock",        "event_fired", "flags",  "item_positions",
        "locked_exits", "npc_states",  "player", "revealed_facts",
    };
    if (const auto issue = exact_fields_issue(document, fields, "world_state")) {
        return std::unexpected(*issue);
    }

    try {
        auto npc_states = document.at("npc_states").get<std::map<std::string, NpcState>>();
        auto flags = document.at("flags").get<std::map<std::string, bool>>();
        auto event_fired = document.at("event_fired").get<std::map<std::string, bool>>();
        auto locked_exits =
            document.at("locked_exits").get<std::map<std::string, std::vector<LockedExitEntry>>>();

        if (!same_keys(canonical.npcs, npc_states)) {
            return std::unexpected("world_state NPC set differs from the cartridge");
        }
        if (!same_keys(canonical.flags, flags)) {
            return std::unexpected("world_state flag set differs from the cartridge");
        }
        if (!same_keys(canonical.events, event_fired)) {
            return std::unexpected("world_state event set differs from the cartridge");
        }
        if (!same_keys(canonical.locations, locked_exits)) {
            return std::unexpected("world_state location set differs from the cartridge");
        }

        WorldState restored = canonical;
        document.at("player").get_to(restored.player);
        document.at("clock").get_to(restored.clock);
        document.at("revealed_facts").get_to(restored.revealed_facts);
        document.at("item_positions").get_to(restored.item_positions);
        restored.flags = std::move(flags);
        for (auto &[npc_id, npc] : restored.npcs) {
            npc.state = std::move(npc_states.at(npc_id));
        }
        for (auto &[event_id, event] : restored.events) {
            event.fired = event_fired.at(event_id);
        }
        for (auto &[location_id, location] : restored.locations) {
            location.locked_exits = std::move(locked_exits.at(location_id));
        }

        if (const auto issue = runtime_state_issue(canonical, restored)) {
            return std::unexpected(*issue);
        }
        return restored;
    } catch (const std::exception &exception) {
        return std::unexpected("invalid world_state: " + std::string(exception.what()));
    }
}

std::optional<std::string> runtime_inconsistency(const WorldState &world, const GamePhase phase,
                                                 const std::optional<std::string> &active_npc) {
    if (phase != GamePhase::in_conversation) {
        return active_npc ? std::optional<std::string>("active_npc requires in_conversation phase")
                          : std::nullopt;
    }
    if (!active_npc) {
        return "in_conversation phase requires active_npc";
    }
    const auto npc = world.npcs.find(*active_npc);
    if (npc == world.npcs.end()) {
        return "active_npc is unknown";
    }
    if (npc->second.state.current_location != world.player.current_location) {
        return "active_npc is not at the player's location";
    }
    return std::nullopt;
}

std::optional<std::string> conversation_inconsistency(const WorldState &world,
                                                      const json &conversations) {
    if (!conversations.is_object()) {
        return "conversations must be an object";
    }
    if (conversations.size() > world.npcs.size()) {
        return "conversations contains more entries than the cartridge has NPCs";
    }
    for (const auto &[npc_id, document] : conversations.items()) {
        if (!world.npcs.contains(npc_id)) {
            return "conversation references unknown NPC '" + npc_id + "'";
        }
        if (!document.is_object()) {
            return "conversation for '" + npc_id + "' must be an object";
        }
    }
    return std::nullopt;
}

std::expected<std::string, SaveError> read_bounded(const fs::path &path) {
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        const auto kind = errno == ENOENT ? SaveError::Kind::missing : SaveError::Kind::io;
        return std::unexpected(
            error(kind, "Could not open save '" + path.string() + "': " + std::strerror(errno)));
    }

    struct stat status{};
    if (::fstat(descriptor, &status) != 0) {
        const int saved_errno = errno;
        (void)::close(descriptor);
        return std::unexpected(
            error(SaveError::Kind::io,
                  "Could not inspect save: " + std::string(std::strerror(saved_errno))));
    }
    if (!S_ISREG(status.st_mode)) {
        (void)::close(descriptor);
        return std::unexpected(error(SaveError::Kind::corrupt, "Save path is not a regular file"));
    }
    if (status.st_size < 0 || static_cast<std::uintmax_t>(status.st_size) > MAX_SAVE_BYTES) {
        (void)::close(descriptor);
        return std::unexpected(
            error(SaveError::Kind::too_large, "Save exceeds the 64 MiB size limit"));
    }

    std::string contents(static_cast<std::size_t>(status.st_size), '\0');
    std::size_t offset = 0;
    while (offset < contents.size()) {
        const auto count = ::read(descriptor, contents.data() + offset, contents.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            const int saved_errno = count == 0 ? EIO : errno;
            (void)::close(descriptor);
            return std::unexpected(
                error(SaveError::Kind::io,
                      "Could not read save: " + std::string(std::strerror(saved_errno))));
        }
        offset += static_cast<std::size_t>(count);
    }
    if (::close(descriptor) != 0) {
        return std::unexpected(error(SaveError::Kind::io,
                                     "Could not close save: " + std::string(std::strerror(errno))));
    }
    return contents;
}

SaveResult write_atomically(const fs::path &directory, const fs::path &target,
                            const std::string &contents) {
    fs::path temporary = target;
    temporary += ".tmp." + std::to_string(::getpid()) + "." +
                 std::to_string(temp_sequence.fetch_add(1, std::memory_order_relaxed));

    const int descriptor =
        ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        return std::unexpected(error(SaveError::Kind::io, "Could not create temporary save: " +
                                                              std::string(std::strerror(errno))));
    }
    const auto fail = [&](const int failure_errno, std::string message) -> SaveResult {
        (void)::close(descriptor);
        std::error_code ignored;
        fs::remove(temporary, ignored);
        return std::unexpected(
            error(SaveError::Kind::io, std::move(message) + ": " + std::strerror(failure_errno)));
    };

    std::size_t offset = 0;
    while (offset < contents.size()) {
        const auto count = ::write(descriptor, contents.data() + offset, contents.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return fail(count == 0 ? EIO : errno, "Could not write temporary save");
        }
        offset += static_cast<std::size_t>(count);
    }
    if (::fsync(descriptor) != 0) {
        return fail(errno, "Could not flush temporary save");
    }
    if (::close(descriptor) != 0) {
        const int saved_errno = errno;
        std::error_code ignored;
        fs::remove(temporary, ignored);
        return std::unexpected(
            error(SaveError::Kind::io,
                  "Could not close temporary save: " + std::string(std::strerror(saved_errno))));
    }
    if (::rename(temporary.c_str(), target.c_str()) != 0) {
        const int saved_errno = errno;
        std::error_code ignored;
        fs::remove(temporary, ignored);
        return std::unexpected(
            error(SaveError::Kind::io,
                  "Could not replace save atomically: " + std::string(std::strerror(saved_errno))));
    }

    const int directory_descriptor = ::open(directory.c_str(), O_RDONLY | O_CLOEXEC);
    if (directory_descriptor < 0) {
        return std::unexpected(
            error(SaveError::Kind::io, "Could not open save directory for flushing: " +
                                           std::string(std::strerror(errno))));
    }
    if (::fsync(directory_descriptor) != 0) {
        const int saved_errno = errno;
        (void)::close(directory_descriptor);
        return std::unexpected(
            error(SaveError::Kind::io,
                  "Could not flush save directory: " + std::string(std::strerror(saved_errno))));
    }
    if (::close(directory_descriptor) != 0) {
        return std::unexpected(error(SaveError::Kind::io, "Could not close save directory: " +
                                                              std::string(std::strerror(errno))));
    }
    return {};
}

} // namespace

SaveSystem::SaveSystem(fs::path directory, const WorldState &canonical_world)
    : directory_(std::move(directory)), canonical_world_(canonical_world),
      cartridge_id_(canonical_world.manifest.id),
      cartridge_version_(canonical_world.manifest.version) {
    if (const auto issue = runtime_state_issue(canonical_world_, canonical_world_)) {
        throw std::invalid_argument("SaveSystem requires a valid canonical world: " + *issue);
    }

    std::error_code ec;
    const auto before = fs::symlink_status(directory_, ec);
    if (!ec && fs::is_symlink(before)) {
        throw std::runtime_error("Save directory must not be a symbolic link: " +
                                 directory_.string());
    }
    ec.clear();
    fs::create_directories(directory_, ec);
    if (ec) {
        throw fs::filesystem_error("Could not create save directory", directory_, ec);
    }
    const auto after = fs::symlink_status(directory_, ec);
    if (ec || !fs::is_directory(after) || fs::is_symlink(after)) {
        throw std::runtime_error("Save path is not a safe directory: " + directory_.string());
    }
    directory_ = fs::canonical(directory_);
}

fs::path SaveSystem::path_for(const int slot) const {
    if (!valid_slot(slot)) {
        throw std::out_of_range("Save slot must be between 1 and 99");
    }
    return directory_ / ("slot_" + std::to_string(slot) + ".json");
}

SaveResult SaveSystem::save(const int slot, const WorldState &world, const GamePhase phase,
                            const std::optional<std::string> &active_npc,
                            const json &conversations) {
    if (!valid_slot(slot)) {
        return std::unexpected(
            error(SaveError::Kind::invalid_slot, "Save slot must be between 1 and 99"));
    }
    if (const auto issue = runtime_state_issue(canonical_world_, world)) {
        return std::unexpected(
            error(SaveError::Kind::corrupt, "Refusing to save invalid runtime state: " + *issue));
    }
    if (const auto issue = runtime_inconsistency(world, phase, active_npc)) {
        return std::unexpected(error(SaveError::Kind::corrupt, *issue));
    }
    if (const auto issue = conversation_inconsistency(world, conversations)) {
        return std::unexpected(error(SaveError::Kind::corrupt, *issue));
    }

    try {
        const json payload{
            {"save_schema_version", SAVE_SCHEMA_VERSION},
            {"cartridge_id", cartridge_id_},
            {"cartridge_version", cartridge_version_},
            {"phase", to_string(phase)},
            {"active_npc", active_npc ? json(*active_npc) : json(nullptr)},
            {"world_state", runtime_state_document(world)},
            {"conversations", conversations},
        };
        std::string encoded = payload.dump(2);
        encoded.push_back('\n');
        if (encoded.size() > MAX_SAVE_BYTES) {
            return std::unexpected(
                error(SaveError::Kind::too_large, "Save exceeds the 64 MiB size limit"));
        }
        return write_atomically(directory_, path_for(slot), encoded);
    } catch (const std::exception &exception) {
        return std::unexpected(error(SaveError::Kind::corrupt,
                                     "Could not serialize save: " + std::string(exception.what())));
    }
}

std::expected<SavePayload, SaveError> SaveSystem::load(const int slot) const {
    if (!valid_slot(slot)) {
        return std::unexpected(
            error(SaveError::Kind::invalid_slot, "Save slot must be between 1 and 99"));
    }
    auto contents = read_bounded(path_for(slot));
    if (!contents) {
        return std::unexpected(std::move(contents.error()));
    }

    json data;
    try {
        data = json::parse(*contents);
    } catch (const json::parse_error &exception) {
        return std::unexpected(
            error(SaveError::Kind::corrupt, "Invalid save JSON: " + std::string(exception.what())));
    }

    static const std::set<std::string> fields = {
        "active_npc", "cartridge_id",        "cartridge_version", "conversations",
        "phase",      "save_schema_version", "world_state",
    };
    if (const auto issue = exact_fields_issue(data, fields, "save document")) {
        return std::unexpected(error(SaveError::Kind::corrupt, *issue));
    }
    if (!data["save_schema_version"].is_number_integer() ||
        data["save_schema_version"].get<int>() != SAVE_SCHEMA_VERSION) {
        return std::unexpected(
            error(SaveError::Kind::unsupported_schema,
                  "Unsupported save schema: " + data["save_schema_version"].dump()));
    }
    if (!data["cartridge_id"].is_string() || !data["cartridge_version"].is_string()) {
        return std::unexpected(
            error(SaveError::Kind::corrupt, "Save cartridge identity must be strings"));
    }
    const auto saved_id = data["cartridge_id"].get<std::string>();
    const auto saved_version = data["cartridge_version"].get<std::string>();
    if (saved_id != cartridge_id_ || saved_version != cartridge_version_) {
        return std::unexpected(error(SaveError::Kind::wrong_cartridge,
                                     "Save belongs to " + saved_id + " version " + saved_version +
                                         ", expected " + cartridge_id_ + " version " +
                                         cartridge_version_));
    }

    try {
        if (!data["phase"].is_string()) {
            return std::unexpected(error(SaveError::Kind::corrupt, "Save phase must be a string"));
        }
        const auto phase = phase_from_string(data["phase"].get<std::string>());
        if (!phase) {
            return std::unexpected(
                error(SaveError::Kind::corrupt, "Save contains an unknown phase"));
        }
        auto world = restore_runtime_state(canonical_world_, data["world_state"]);
        if (!world) {
            return std::unexpected(error(SaveError::Kind::corrupt, std::move(world.error())));
        }

        SavePayload payload{
            .world = std::move(*world),
            .phase = *phase,
            .active_npc = std::nullopt,
            .conversations = json::object(),
        };
        if (data["active_npc"].is_string()) {
            payload.active_npc = data["active_npc"].get<std::string>();
        } else if (!data["active_npc"].is_null()) {
            return std::unexpected(
                error(SaveError::Kind::corrupt, "active_npc must be a string or null"));
        }
        if (!data["conversations"].is_object()) {
            return std::unexpected(
                error(SaveError::Kind::corrupt, "conversations must be an object"));
        }
        payload.conversations = data["conversations"];

        if (const auto issue =
                runtime_inconsistency(payload.world, payload.phase, payload.active_npc)) {
            return std::unexpected(error(SaveError::Kind::corrupt, *issue));
        }
        if (const auto issue = conversation_inconsistency(payload.world, payload.conversations)) {
            return std::unexpected(error(SaveError::Kind::corrupt, *issue));
        }
        return payload;
    } catch (const std::exception &exception) {
        return std::unexpected(error(SaveError::Kind::corrupt,
                                     "Invalid save payload: " + std::string(exception.what())));
    }
}

} // namespace chronicle
