// Per-NPC scry sessions.
//
// Each NPC that the player talks to gets its own scry::Harness (registering
// exactly the tools its cartridge policy allows, with schemas derived from
// annotated adapter aggregates via C++26 reflection) and its own persistent
// scry::Conversation. Tool handlers run on this thread inside the harness turn
// and route through the game's action gate. The entire world turn commits only
// together with the matching conversation turn.
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
// Returns nullopt when the host did not configure an endpoint, and what is wrong
// when it configured half of one or one no request could be sent to.
[[nodiscard]] std::expected<std::optional<EndpointConfig>, std::string>
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
    virtual ~NpcSessionManager();

    NpcSessionManager(const NpcSessionManager &) = delete;
    NpcSessionManager &operator=(const NpcSessionManager &) = delete;

    // Run one blocking conversation turn with the active NPC. A failed turn
    // rolls back every tool-side world write and returns an error so the
    // console can provide deterministic stub dialogue. A turn whose NPC has
    // already spoken or visibly acted is complete, not failed, when the model's
    // next response is empty or broken; before that, the model is asked once more.
    [[nodiscard]] NpcTurnResult run_turn(const std::string &npc_id, const std::string &player_text);

    // npc_id -> serialized scry conversation document, for save files.
    [[nodiscard]] std::expected<nlohmann::json, std::string> snapshot_conversations();

    // Validate every restored document, under the cartridge's own system prompt,
    // before atomically replacing live sessions. Each document is picked up when
    // the player next talks to that NPC.
    [[nodiscard]] std::expected<void, std::string>
    restore_conversations(const nlohmann::json &conversations);

  protected:
    // The one call a model tool makes into the game. Virtual only so that tests
    // can make it throw mid-turn.
    [[nodiscard]] virtual ToolOutcome submit_tool(const std::string &npc_id,
                                                  const tools::NpcToolCall &call);

  private:
    struct Session;

    struct FailedAttempt {
        std::string reason;
        // The model's own response could not be used; the provider is fine.
        bool model_at_fault = false;
    };

    Session &get_or_create(const std::string &npc_id);
    // What an NPC put into words without the say tool.
    struct SpokenText {
        std::string beside_tools;
        std::string closing;
    };
    // One request-and-tools run of a turn. Its tool events and speech are left in
    // turn_events_ and turn_speech_; it returns the NPC's other words, with the
    // history committed, or why nothing came of it.
    [[nodiscard]] std::expected<SpokenText, FailedAttempt> attempt_turn(Session &session,
                                                                        const std::string &message);
    // Why a further line of speech this turn is refused, if it is.
    [[nodiscard]] std::optional<std::string> speech_issue(const std::string &text) const;

    CartridgeGame &game_;
    EndpointConfig endpoint_;
    std::map<std::string, std::unique_ptr<Session>> sessions_;
    std::map<std::string, nlohmann::json> pending_restore_;

    // Buffered gate output for the in-flight turn (filled by tool handlers).
    GameEvents turn_events_;
    // What the NPC has said through the say tool this turn, as shown.
    std::vector<std::string> turn_speech_;
    // Why a throwing tool handler cancelled the in-flight turn.
    std::optional<std::string> turn_fault_;
    std::uint64_t use_sequence_ = 0;
};

} // namespace chronicle
