// Versioned save/load bound to a cartridge id/version.
#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include "chronicle/cartridge/models.hpp"
#include "chronicle/types.hpp"

namespace chronicle {

inline constexpr int SAVE_SCHEMA_VERSION = 1;

struct SavePayload {
    WorldState world;
    GamePhase phase = GamePhase::playing;
    std::optional<std::string> active_npc;
    // Serialized scry conversations, npc_id -> conversation document.
    nlohmann::json conversations = nlohmann::json::object();
};

struct SaveError {
    enum class Kind { missing, unsupported_schema, corrupt } kind;
    std::string message;
};

class SaveSystem {
  public:
    explicit SaveSystem(std::filesystem::path directory);

    [[nodiscard]] std::filesystem::path path_for(int slot) const;

    void save(int slot, const WorldState &world, GamePhase phase,
              const std::optional<std::string> &active_npc,
              const nlohmann::json &conversations = nlohmann::json::object());

    [[nodiscard]] std::expected<SavePayload, SaveError> load(int slot) const;

  private:
    std::filesystem::path directory_;
};

} // namespace chronicle
