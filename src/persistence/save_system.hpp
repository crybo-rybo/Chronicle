#pragma once
#include "entities/world.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace chronicle {

struct SaveData {
    static constexpr int kCurrentSchemaVersion = 1;
    int schema_version = kCurrentSchemaVersion;
    World world;
};

struct SaveSlotInfo {
    int slot;
    std::string timestamp;
    std::string location_name;
    std::string clock_display;
};

class SaveSystem {
  public:
    explicit SaveSystem(std::filesystem::path save_dir);

    void save(const World &world, int slot);
    std::optional<SaveData> load(int slot) const;
    std::vector<SaveSlotInfo> list_slots() const;
    bool slot_exists(int slot) const;
    void delete_slot(int slot);

  private:
    std::filesystem::path save_dir_;
    std::filesystem::path slot_path(int slot) const;
};

} // namespace chronicle
