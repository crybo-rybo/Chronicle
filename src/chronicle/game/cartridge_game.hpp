// Default cartridge-backed mystery/social-sim game.
//
// Player commands, scripted events, persistence restores, and model tools all
// translate into typed world actions applied by one mutation gate.
#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "chronicle/cartridge/models.hpp"
#include "chronicle/game/npc_tools.hpp"
#include "chronicle/game/world_actions.hpp"
#include "chronicle/persist.hpp"
#include "chronicle/types.hpp"

namespace chronicle {

struct ToolRejection {
    std::string reason;
};

using ToolOutcome = std::expected<GameEvents, ToolRejection>;

struct NpcTurnRequest {
    std::string npc_id;
    std::string player_text;
};

struct PlayerDispatch {
    GameEvents events;
    std::optional<NpcTurnRequest> npc_turn;
};

// Complete deterministic runtime checkpoint used to make a model turn atomic.
struct RuntimeCheckpoint {
    WorldState world;
    GamePhase phase = GamePhase::playing;
    std::optional<std::string> active_npc;
    bool significant = false;
};

class CartridgeGame {
  public:
    // Extra payload persisted alongside the world (NPC conversation history).
    using SnapshotHook = std::function<nlohmann::json()>;
    using RestoreHook = std::function<void(const nlohmann::json &)>;

    explicit CartridgeGame(const std::filesystem::path &package_dir,
                           std::optional<std::filesystem::path> save_dir = std::nullopt);
    CartridgeGame(WorldState world, std::optional<std::filesystem::path> save_dir = std::nullopt);

    CartridgeGame(const CartridgeGame &) = delete;
    CartridgeGame &operator=(const CartridgeGame &) = delete;
    CartridgeGame(CartridgeGame &&) = delete;
    CartridgeGame &operator=(CartridgeGame &&) = delete;

    [[nodiscard]] GamePhase phase() const { return phase_; }
    [[nodiscard]] const std::optional<std::string> &active_npc_id() const { return active_npc_; }
    [[nodiscard]] const WorldState &world() const { return world_; }

    [[nodiscard]] GameEvents bootstrap();
    [[nodiscard]] PlayerDispatch dispatch_player(const std::string &text);

    // The only accepted path for runtime world writes. Public so an optional
    // GameBackend and deterministic tests can submit the same typed actions.
    [[nodiscard]] actions::ActionOutcome submit_world_action(actions::WorldAction action);

    [[nodiscard]] RuntimeCheckpoint checkpoint_runtime() const;
    [[nodiscard]] actions::ActionOutcome restore_runtime(RuntimeCheckpoint checkpoint);

    // NPC authorization boundary: validate policy, then translate the tool to
    // typed world actions handled by the single mutation gate.
    [[nodiscard]] ToolOutcome submit_npc_tool(const std::string &npc_id,
                                              const tools::NpcToolCall &call);

    // Clock advance + scripted events after a significant turn.
    [[nodiscard]] GameEvents after_turn();

    void save(int slot);
    [[nodiscard]] std::string help_text() const;

    // Wired by the runtime so saves capture NPC conversations; optional.
    void set_conversation_hooks(SnapshotHook snapshot, RestoreHook restore);

  private:
    [[nodiscard]] GameEvents dispatch_playing(const std::string &text);
    [[nodiscard]] std::string expand_alias(const std::string &text) const;

    [[nodiscard]] GameEvents look() const;
    [[nodiscard]] GameEvents show_inventory() const;
    [[nodiscard]] GameEvents go(const std::string &direction);
    [[nodiscard]] GameEvents examine(const std::string &arg) const;
    [[nodiscard]] GameEvents take(const std::string &arg);
    [[nodiscard]] GameEvents drop(const std::string &arg);
    [[nodiscard]] GameEvents use(const std::string &arg);
    [[nodiscard]] GameEvents talk(const std::string &arg);
    [[nodiscard]] GameEvents do_load(int slot);

    [[nodiscard]] std::optional<std::string> resolve_item_ref(const std::string &text) const;
    [[nodiscard]] std::optional<std::string> resolve_npc_ref(const std::string &text) const;
    [[nodiscard]] bool item_accessible(const std::string &item_id) const;

    // Gate internals: empty optional means valid.
    [[nodiscard]] std::optional<std::string>
    validate_npc_tool(const std::string &npc_id, const tools::NpcToolCall &call) const;
    [[nodiscard]] ToolOutcome apply_npc_tool(const std::string &npc_id,
                                             const tools::NpcToolCall &call);
    [[nodiscard]] std::optional<GameEvent>
    narrate(const std::string &key, const std::map<std::string, std::string> &args) const;

    [[nodiscard]] GameEvents evaluate_events();
    [[nodiscard]] bool condition_met(const ConditionData &cond) const;
    [[nodiscard]] GameEvents apply_event_action(const EventActionData &action);

    WorldState world_;
    GamePhase phase_ = GamePhase::playing;
    std::optional<std::string> active_npc_;
    bool significant_ = false;
    actions::ActionGate action_gate_;
    SaveSystem saves_;
    SnapshotHook conversation_snapshot_;
    RestoreHook conversation_restore_;
};

} // namespace chronicle
