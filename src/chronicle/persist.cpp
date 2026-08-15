#include "chronicle/persist.hpp"

#include <fstream>

namespace chronicle {

namespace fs = std::filesystem;
using nlohmann::json;

SaveSystem::SaveSystem(fs::path directory) : directory_(std::move(directory)) {
    fs::create_directories(directory_);
}

fs::path SaveSystem::path_for(const int slot) const {
    return directory_ / ("slot_" + std::to_string(slot) + ".json");
}

void SaveSystem::save(const int slot, const WorldState &world, const GamePhase phase,
                      const std::optional<std::string> &active_npc, const json &conversations) {
    json payload{
        {"save_schema_version", SAVE_SCHEMA_VERSION},
        {"cartridge_id", world.manifest.id},
        {"cartridge_version", world.manifest.version},
        {"phase", to_string(phase)},
        {"active_npc", active_npc ? json(*active_npc) : json(nullptr)},
        {"world", world},
        {"conversations", conversations},
    };
    std::ofstream stream(path_for(slot));
    stream << payload.dump(2) << '\n';
}

std::expected<SavePayload, SaveError> SaveSystem::load(const int slot) const {
    const fs::path path = path_for(slot);
    std::ifstream stream(path);
    if (!stream) {
        return std::unexpected(SaveError{.kind = SaveError::Kind::missing,
                                         .message = "No save file: " + path.string()});
    }
    json data;
    try {
        data = json::parse(stream);
    } catch (const json::parse_error &exc) {
        return std::unexpected(SaveError{.kind = SaveError::Kind::corrupt, .message = exc.what()});
    }
    if (data.value("save_schema_version", -1) != SAVE_SCHEMA_VERSION) {
        return std::unexpected(
            SaveError{.kind = SaveError::Kind::unsupported_schema,
                      .message = "Unsupported save schema: " +
                                 data.value("save_schema_version", json(nullptr)).dump()});
    }
    try {
        SavePayload payload;
        data.at("world").get_to(payload.world);
        payload.phase = phase_from_string(data.value("phase", "playing"));
        const auto npc = data.find("active_npc");
        if (npc != data.end() && npc->is_string()) {
            payload.active_npc = npc->get<std::string>();
        }
        const auto convos = data.find("conversations");
        if (convos != data.end() && convos->is_object()) {
            payload.conversations = *convos;
        }
        return payload;
    } catch (const std::exception &exc) {
        return std::unexpected(SaveError{.kind = SaveError::Kind::corrupt, .message = exc.what()});
    }
}

} // namespace chronicle
