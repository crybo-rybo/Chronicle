// Per-NPC scry sessions.
//
// Each NPC that the player talks to gets its own scry::Harness (registering
// exactly the tools its cartridge policy allows, schemas derived from the
// npc_tools.hpp aggregates via C++26 reflection) and its own persistent
// scry::Conversation. Tool handlers run on this thread inside the harness
// turn and route through the game's action gate; a gate rejection is returned
// to the model as a tool error it can react to.
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "chronicle/cartridge/models.hpp"
#include "chronicle/game/cartridge_game.hpp"
#include "chronicle/types.hpp"

namespace chronicle {

struct EndpointConfig {
    std::string base_url;
    std::string model;
    std::string api_key = "not-needed";
    bool anthropic_dialect = false;
    bool disable_reasoning = false;
    double temperature = 0.7;
    int max_tokens = 512;
    int timeout_ms = 120'000;

    [[nodiscard]] bool configured() const { return !base_url.empty() && !model.empty(); }
};

// Resolution order: CLI flags, CHRONICLE_* environment variables, then the
// cartridge's own config endpoint. Sampling limits always come from the
// cartridge config when one is present. Returns nullopt when no endpoint is
// configured anywhere (the console then degrades to the stub).
[[nodiscard]] std::optional<EndpointConfig>
resolve_endpoint(const std::optional<std::string> &base_url_flag,
                 const std::optional<std::string> &model_flag, const ConfigData *cartridge_config);

class NpcSessionManager {
  public:
    NpcSessionManager(CartridgeGame &game, EndpointConfig endpoint);
    ~NpcSessionManager();

    NpcSessionManager(const NpcSessionManager &) = delete;
    NpcSessionManager &operator=(const NpcSessionManager &) = delete;

    // Run one blocking conversation turn with the active NPC. World changes
    // applied by tool calls are returned as events; provider failures come
    // back as a warning event, never an exception.
    [[nodiscard]] GameEvents run_turn(const std::string &npc_id, const std::string &player_text);

    // npc_id -> serialized scry conversation document, for save files.
    [[nodiscard]] nlohmann::json snapshot_conversations();

    // Drop live sessions and stage restored conversation documents; each is
    // picked up when the player next talks to that NPC.
    void restore_conversations(const nlohmann::json &conversations);

  private:
    struct Session;

    Session &get_or_create(const std::string &npc_id);

    CartridgeGame &game_;
    EndpointConfig endpoint_;
    std::map<std::string, std::unique_ptr<Session>> sessions_;
    std::map<std::string, nlohmann::json> pending_restore_;

    // Buffered gate output for the in-flight turn (filled by tool handlers).
    GameEvents turn_events_;
    bool turn_had_dialogue_ = false;
};

} // namespace chronicle
