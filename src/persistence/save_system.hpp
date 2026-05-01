/**
 * @file save_system.hpp
 * @brief Save and load game state for Chronicle.
 *
 * @details @ref SaveSystem serialises the entire @ref World to a JSON file and
 * deserialises it back, providing numbered save slots.  Each slot maps to a
 * file named @c slot_N.json in the configured save directory.
 *
 * ### Save file format
 * @code{.json}
 * {
 *   "version": 1,
 *   "metadata": {
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
 * Runtime-only objects are intentionally excluded: Zoo-Keeper agents and KV
 * cache, agent history, active conversation handles, pending mutation queues,
 * token queues, renderer state, and open input streams.  Agents are re-created
 * fresh on load; the NPC memory system (which IS saved as part of
 * @ref NpcState) provides conversation continuity.
 */

#pragma once
#include "entities/world.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace chronicle {

/// @brief The data loaded from a save file.
struct SaveData {
    /// @brief Current save file schema version.
    ///
    /// Increment this constant when making backwards-incompatible changes to
    /// the save format.
    static constexpr int kCurrentSchemaVersion = 1;

    int schema_version = kCurrentSchemaVersion; ///< Version read from the file.
    World world;                                ///< The deserialised world state.
};

/// @brief Metadata about a single save slot, used for listing saves.
struct SaveSlotInfo {
    int slot;                  ///< Slot number.
    std::string timestamp;     ///< ISO-8601 timestamp string from the save metadata.
    std::string location_name; ///< Display name of the player's location at save time.
    std::string clock_display; ///< Formatted clock string at save time (e.g. "Afternoon, Day 2").
};

/// @brief Manages numbered save slots backed by JSON files on disk.
class SaveSystem {
  public:
    /// @brief Construct with the directory to use for save files.
    ///
    /// @details The directory is created lazily on the first @ref save call.
    /// @param save_dir Path to the save directory.
    explicit SaveSystem(std::filesystem::path save_dir);

    /// @brief Serialise the world to a numbered save slot.
    ///
    /// @details Creates @c save_dir_ if it does not exist, then writes a JSON
    /// file containing the schema version, metadata, and serialised world.
    ///
    /// @param world The world state to save.
    /// @param slot  The slot number.  Overwrites any existing save in that slot.
    /// @throws std::runtime_error if the file cannot be opened or written.
    void save(const World &world, int slot);

    /// @brief Load a world state from a numbered save slot.
    ///
    /// @details Returns @c std::nullopt if the slot file does not exist, cannot
    /// be opened, fails to parse as JSON, or has a schema version that does not
    /// match @ref SaveData::kCurrentSchemaVersion.  This function only
    /// deserializes durable data; @ref GameEngine is responsible for clearing
    /// runtime-only state such as active conversations, token queues, and
    /// pending mutations after replacing its world.
    ///
    /// @param slot The slot number to load from.
    /// @return The loaded @ref SaveData, or @c std::nullopt on any failure.
    std::optional<SaveData> load(int slot) const;

    /// @brief Enumerate all valid save slots in the save directory.
    ///
    /// @details Scans the save directory for files matching the @c slot_N.json
    /// pattern, reads their metadata, and returns the results sorted by slot
    /// number.  Files that cannot be parsed are silently skipped.
    ///
    /// @return A vector of @ref SaveSlotInfo objects, sorted ascending by slot number.
    std::vector<SaveSlotInfo> list_slots() const;

    /// @brief Test whether a given slot file exists on disk.
    /// @param slot The slot number to test.
    /// @return @c true if the file @c slot_N.json exists.
    bool slot_exists(int slot) const;

    /// @brief Delete the save file for the given slot.
    ///
    /// @details If the file does not exist, the call is a no-op.
    /// @param slot The slot number to delete.
    void delete_slot(int slot);

  private:
    std::filesystem::path save_dir_; ///< Root directory for save files.

    /// @brief Compute the full path for a slot's save file.
    /// @param slot The slot number.
    /// @return Path of the form @c save_dir_/slot_N.json.
    std::filesystem::path slot_path(int slot) const;
};

} // namespace chronicle
