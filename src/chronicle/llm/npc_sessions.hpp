// Per-NPC scry sessions.
//
// Each NPC that the player talks to gets its own scry::Harness (registering
// exactly the tools its cartridge policy allows, with schemas derived from
// annotated adapter aggregates via C++26 reflection) and its own persistent
// scry::Conversation. Tool handlers run on this thread inside the harness turn
// and route through the game's action gate. The entire world turn commits only
// if Scry commits the matching conversation turn.
#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

// Endpoint authority comes only from host-controlled CLI flags and
// CHRONICLE_* environment variables. Cartridges may tune bounded sampling
// values, but cannot select a network destination or receive host credentials.
// Returns nullopt when the host did not configure an endpoint.
[[nodiscard]] std::optional<EndpointConfig>
resolve_endpoint(const std::optional<std::string> &base_url_flag,
                 const std::optional<std::string> &model_flag, const ConfigData *cartridge_config);

struct ReflectedToolSchema {
    std::string name;
    std::string description;
    std::string input_schema;
};

// Exposes the actual reflected schemas for contract tests and diagnostics.
[[nodiscard]] std::vector<ReflectedToolSchema> npc_tool_schemas();

struct NpcTurnFailure {
    std::string message;
    bool world_rolled_back = true;
};

using NpcTurnResult = std::expected<GameEvents, NpcTurnFailure>;

class NpcSessionManager {
  public:
    NpcSessionManager(CartridgeGame &game, EndpointConfig endpoint);
    ~NpcSessionManager();

    NpcSessionManager(const NpcSessionManager &) = delete;
    NpcSessionManager &operator=(const NpcSessionManager &) = delete;

    // Run one blocking conversation turn with the active NPC. Provider failure
    // rolls back every tool mutation and returns an error so the console can
    // provide deterministic stub dialogue.
    [[nodiscard]] NpcTurnResult run_turn(const std::string &npc_id, const std::string &player_text);

    // npc_id -> serialized scry conversation document, for save files.
    [[nodiscard]] std::expected<nlohmann::json, std::string> snapshot_conversations();

    // Validate every restored document and its canonical system prompt before
    // atomically replacing live sessions. Each document is picked up when the
    // player next talks to that NPC.
    [[nodiscard]] std::expected<void, std::string>
    restore_conversations(const nlohmann::json &conversations);

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
    std::uint64_t use_sequence_ = 0;
};

} // namespace chronicle
