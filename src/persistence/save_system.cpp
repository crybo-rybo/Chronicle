/**
 * @file save_system.cpp
 * @brief Implementation of @ref SaveSystem — JSON-based save/load for game state.
 */

#include "persistence/save_system.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string_view>

namespace chronicle {
namespace {

void validate_path_segment(std::string_view value, std::string_view label) {
    if (value.empty() || value == "." || value == ".." ||
        value.find('/') != std::string_view::npos || value.find('\\') != std::string_view::npos) {
        throw std::invalid_argument(std::string(label) +
                                    " must be a safe single path segment: " + std::string(value));
    }
}

} // namespace

SaveSystem::SaveSystem(std::filesystem::path save_dir, std::optional<CartridgeBinding> cartridge)
    : save_dir_(std::move(save_dir)), cartridge_(std::move(cartridge)) {
    slot_root_ = save_dir_;
    if (cartridge_) {
        validate_path_segment(cartridge_->scenario_id, "Scenario id");
        slot_root_ /= cartridge_->scenario_id;
    }
}

std::filesystem::path SaveSystem::slot_path(int slot) const {
    return slot_root_ / ("slot_" + std::to_string(slot) + ".json");
}

bool SaveSystem::cartridge_metadata_matches(const nlohmann::json &metadata) const {
    if (!cartridge_) {
        return true;
    }

    if (!metadata.contains("scenario_id")) {
        return false;
    }

    if (metadata.at("scenario_id").get<std::string>() != cartridge_->scenario_id) {
        return false;
    }

    if (metadata.contains("scenario_version") &&
        metadata.at("scenario_version").get<std::string>() != cartridge_->scenario_version) {
        return false;
    }

    if (metadata.contains("chronicle_schema_version") &&
        metadata.at("chronicle_schema_version").get<int>() !=
            cartridge_->chronicle_schema_version) {
        return false;
    }

    return true;
}

void SaveSystem::save(const World &world, int slot) {
    std::filesystem::create_directories(slot_root_);

    std::string location_name = world.player.current_location;
    auto it = world.locations.find(world.player.current_location);
    if (it != world.locations.end()) {
        location_name = it->second.name;
    }

    auto now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm buf{};
    localtime_r(&now_time, &buf);
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", &buf);
    std::string timestamp(time_str);

    nlohmann::json metadata = {
        {"location", location_name},
        {"clock", world.clock.to_display_string()},
        {"timestamp", timestamp},
    };
    if (cartridge_) {
        metadata["scenario_id"] = cartridge_->scenario_id;
        metadata["scenario_version"] = cartridge_->scenario_version;
        metadata["chronicle_schema_version"] = cartridge_->chronicle_schema_version;
    }

    nlohmann::json j;
    j["version"] = SaveData::kCurrentSchemaVersion;
    j["metadata"] = std::move(metadata);
    j["world"] = world;

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

        if (cartridge_ && (!j.contains("metadata") || !j.at("metadata").is_object() ||
                           !cartridge_metadata_matches(j.at("metadata")))) {
            return std::nullopt;
        }

        if (!cartridge_ && j.contains("metadata") && !j.at("metadata").is_object()) {
            return std::nullopt;
        }

        SaveData data;
        data.schema_version = j["version"].get<int>();
        data.world = j["world"].get<World>();
        if (j.contains("metadata") && j.at("metadata").contains("scenario_id")) {
            CartridgeBinding binding;
            binding.scenario_id = j.at("metadata").at("scenario_id").get<std::string>();
            if (j.at("metadata").contains("scenario_version")) {
                binding.scenario_version =
                    j.at("metadata").at("scenario_version").get<std::string>();
            }
            if (j.at("metadata").contains("chronicle_schema_version")) {
                binding.chronicle_schema_version =
                    j.at("metadata").at("chronicle_schema_version").get<int>();
            }
            data.cartridge = std::move(binding);
        }
        return data;
    } catch (const nlohmann::json::exception &) {
        return std::nullopt;
    }
}

std::vector<SaveSlotInfo> SaveSystem::list_slots() const {
    if (!std::filesystem::exists(slot_root_)) {
        return {};
    }

    static const std::regex slot_regex(R"(slot_(\d+)\.json)");
    std::vector<SaveSlotInfo> slots;

    for (const auto &entry : std::filesystem::directory_iterator(slot_root_)) {
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
                info.scenario_id = meta.value("scenario_id", "");
            }

            slots.push_back(std::move(info));
        } catch (const nlohmann::json::exception &) {
            continue;
        }
    }

    std::ranges::sort(slots,
                      [](const SaveSlotInfo &a, const SaveSlotInfo &b) { return a.slot < b.slot; });

    return slots;
}

bool SaveSystem::slot_exists(int slot) const {
    return std::filesystem::exists(slot_path(slot));
}

void SaveSystem::delete_slot(int slot) {
    std::filesystem::remove(slot_path(slot));
}

} // namespace chronicle
