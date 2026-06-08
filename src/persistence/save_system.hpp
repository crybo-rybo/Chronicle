/**
 * @file save_system.hpp
 * @brief Save and load game state for Chronicle.
 *
 * @details @ref SaveSystem serialises the entire @ref World to a JSON file and
 * deserialises it back, providing numbered save slots.  When a
 * @ref CartridgeBinding is supplied, saves are namespaced under
 * @c saves/<scenario_id>/slot_N.json and tagged with cartridge identity
 * metadata so incompatible loads are rejected.
 *
 * ### Save file format
 * @code{.json}
 * {
 *   "version": 1,
 *   "metadata": {
 *     "scenario_id": "broken_wheel_sample",
 *     "scenario_version": "1.0.0",
 *     "chronicle_schema_version": 1,
 *     "location": "The Tavern",
 *     "clock": "Afternoon, Day 2",
 *     "timestamp": "2026-04-12T15:30:00"
 *   },
 *   "world": { ... }
 * }
 * @endcode
 *
 * The schema version is checked during load; files with a different version
 * are rejected and @c std::nullopt is returned.
 *
 * ### What is NOT saved
 * Runtime-only objects are intentionally excluded: harness agents, agent
 * history, active conversation handles, pending mutation queues, token queues,
 * renderer state, and open input streams. Agents are re-created fresh on load;
 * the NPC memory system (which IS saved as part of
 * @ref NpcState) provides conversation continuity.
 */

#pragma once
#include "entities/world.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace chronicle {

/// @brief Cartridge identity stored in save metadata and used for slot namespacing.
struct CartridgeBinding {
    std::string scenario_id;
    std::string scenario_version;
    int chronicle_schema_version = 1;
};

/// @brief The data loaded from a save file.
struct SaveData {
    /// @brief Current save file schema version.
    ///
    /// Increment this constant when making backwards-incompatible changes to
    /// the save format.
    static constexpr int kCurrentSchemaVersion = 1;

    int schema_version = kCurrentSchemaVersion; ///< Version read from the file.
    World world;                                ///< The deserialised world state.
    std::optional<CartridgeBinding> cartridge;  ///< Cartridge identity when present.
};

/// @brief Metadata about a single save slot, used for listing saves.
struct SaveSlotInfo {
    int slot;                  ///< Slot number.
    std::string timestamp;     ///< ISO-8601 timestamp string from the save metadata.
    std::string location_name; ///< Display name of the player's location at save time.
    std::string clock_display; ///< Formatted clock string at save time (e.g. "Afternoon, Day 2").
    std::string scenario_id;   ///< Cartridge ID when the save was created.
};

/// @brief Manages numbered save slots backed by JSON files on disk.
class SaveSystem {
  public:
    /// @brief Construct with the base save directory and optional cartridge binding.
    ///
    /// @details When @p cartridge is set, slot files are stored under
    /// @c save_dir/<scenario_id>/slot_N.json.  The directory is created lazily
    /// on the first @ref save call.
    explicit SaveSystem(std::filesystem::path save_dir,
                        std::optional<CartridgeBinding> cartridge = std::nullopt);

    /// @brief Serialise the world to a numbered save slot.
    ///
    /// @throws std::runtime_error if the file cannot be opened or written.
    void save(const World &world, int slot);

    /// @brief Load a world state from a numbered save slot.
    ///
    /// @details Returns @c std::nullopt if the slot file does not exist, cannot
    /// be opened, fails to parse as JSON, has a schema version mismatch, or
    /// whose cartridge metadata does not match the active binding.
    std::optional<SaveData> load(int slot) const;

    /// @brief Enumerate all valid save slots in the save directory.
    std::vector<SaveSlotInfo> list_slots() const;

    /// @brief Test whether a given slot file exists on disk.
    bool slot_exists(int slot) const;

    /// @brief Delete the save file for the given slot.
    void delete_slot(int slot);

    /// @brief Active cartridge binding, if any.
    const std::optional<CartridgeBinding> &cartridge_binding() const noexcept { return cartridge_; }

  private:
    std::filesystem::path save_dir_;  ///< Base save directory from config.
    std::filesystem::path slot_root_; ///< Directory containing slot files.
    std::optional<CartridgeBinding> cartridge_;

    std::filesystem::path slot_path(int slot) const;
    bool cartridge_metadata_matches(const nlohmann::json &metadata) const;
};

} // namespace chronicle
