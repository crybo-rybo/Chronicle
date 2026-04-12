#include "persistence/save_system.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <ranges>
#include <regex>
#include <stdexcept>

namespace chronicle {

SaveSystem::SaveSystem(std::filesystem::path save_dir) : save_dir_(std::move(save_dir)) {}

std::filesystem::path SaveSystem::slot_path(int slot) const {
    return save_dir_ / ("slot_" + std::to_string(slot) + ".json");
}

void SaveSystem::save(const World &world, int slot) {
    std::filesystem::create_directories(save_dir_);

    // Resolve location name from player's current location
    std::string location_name = world.player.current_location;
    auto it = world.locations.find(world.player.current_location);
    if (it != world.locations.end()) {
        location_name = it->second.name;
    }

    // Build timestamp (localtime_r is thread-safe, unlike std::localtime)
    auto now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm buf{};
    localtime_r(&now_time, &buf);
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", &buf);
    std::string timestamp(time_str);

    // Build full JSON
    nlohmann::json j;
    j["version"] = SaveData::kCurrentSchemaVersion;
    j["metadata"] = {
        {"location", location_name},
        {"clock", world.clock.to_display_string()},
        {"timestamp", timestamp},
    };
    j["world"] = world;

    // Write to file
    std::ofstream out(slot_path(slot));
    if (!out) {
        throw std::runtime_error("Failed to open save file: " + slot_path(slot).string());
    }
    out << j.dump(2);
    if (!out) {
        throw std::runtime_error("Failed to write save file: " + slot_path(slot).string());
    }
}

std::optional<SaveData> SaveSystem::load(int slot) const {
    auto path = slot_path(slot);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    try {
        std::ifstream in(path);
        if (!in) {
            return std::nullopt;
        }

        auto j = nlohmann::json::parse(in);

        if (!j.contains("version") || j["version"].get<int>() != SaveData::kCurrentSchemaVersion) {
            return std::nullopt;
        }

        SaveData data;
        data.schema_version = j["version"].get<int>();
        data.world = j["world"].get<World>();
        return data;
    } catch (const nlohmann::json::exception &) {
        return std::nullopt;
    }
}

std::vector<SaveSlotInfo> SaveSystem::list_slots() const {
    if (!std::filesystem::exists(save_dir_)) {
        return {};
    }

    std::regex slot_regex(R"(slot_(\d+)\.json)");
    std::vector<SaveSlotInfo> slots;

    for (const auto &entry : std::filesystem::directory_iterator(save_dir_)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string filename = entry.path().filename().string();
        std::smatch match;
        if (!std::regex_match(filename, match, slot_regex)) {
            continue;
        }

        int slot_num = std::stoi(match[1].str());

        try {
            std::ifstream in(entry.path());
            if (!in) {
                continue;
            }

            auto j = nlohmann::json::parse(in);

            SaveSlotInfo info;
            info.slot = slot_num;

            if (j.contains("metadata")) {
                auto &meta = j["metadata"];
                info.timestamp = meta.value("timestamp", "");
                info.location_name = meta.value("location", "");
                info.clock_display = meta.value("clock", "");
            }

            slots.push_back(std::move(info));
        } catch (const nlohmann::json::exception &) {
            // Skip unparseable files
            continue;
        }
    }

    std::ranges::sort(slots,
                      [](const SaveSlotInfo &a, const SaveSlotInfo &b) { return a.slot < b.slot; });

    return slots;
}

bool SaveSystem::slot_exists(int slot) const { return std::filesystem::exists(slot_path(slot)); }

void SaveSystem::delete_slot(int slot) { std::filesystem::remove(slot_path(slot)); }

} // namespace chronicle
