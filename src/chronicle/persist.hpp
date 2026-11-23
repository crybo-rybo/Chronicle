// Versioned save/load bound to a cartridge id/version.
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include "chronicle/cartridge/models.hpp"
#include "chronicle/types.hpp"

namespace chronicle {

inline constexpr int SAVE_SCHEMA_VERSION = 1;
inline constexpr int MIN_SAVE_SLOT = 1;
inline constexpr int MAX_SAVE_SLOT = 99;
inline constexpr std::uintmax_t MAX_SAVE_BYTES = std::uintmax_t{64} * 1024 * 1024;

struct SavePayload {
    WorldState world;
    GamePhase phase = GamePhase::playing;
    std::optional<std::string> active_npc;
    // Serialized scry conversations, npc_id -> conversation document (scry's
    // version 1 format, which v0.1.0 wrote and v0.8.0 still reads and writes).
    nlohmann::json conversations = nlohmann::json::object();
};

struct SaveError {
    enum class Kind {
        missing,
        invalid_slot,
        unsupported_schema,
        wrong_cartridge,
        too_large,
        corrupt,
        occupied,
        io,
    } kind;
    std::string message;
};

using SaveResult = std::expected<void, SaveError>;

class SaveSystem {
  public:
    // Touches no files; the directory is created by the first save.
    SaveSystem(std::filesystem::path directory, const WorldState &canonical_world);

    [[nodiscard]] std::filesystem::path path_for(int slot) const;

    // Replaces a save of this cartridge, and refuses to replace any other file.
    [[nodiscard]] SaveResult save(int slot, const WorldState &world, GamePhase phase,
                                  const std::optional<std::string> &active_npc,
                                  const nlohmann::json &conversations = nlohmann::json::object());

    [[nodiscard]] std::expected<SavePayload, SaveError> load(int slot) const;

  private:
    std::filesystem::path directory_;
    WorldState canonical_world_;
    std::string cartridge_id_;
    std::string cartridge_version_;
};

} // namespace chronicle
