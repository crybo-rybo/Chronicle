// Default cartridge-backed mystery/social-sim game.
//
// All world mutation driven by the model flows through submit_npc_tool(),
// the single action gate: it validates a typed tool call against the world
// and the NPC's tool policy, then applies it. A rejection carries the reason
// so the LLM layer can surface it back to the model as a tool error.
#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "chronicle/cartridge/models.hpp"
#include "chronicle/game/npc_tools.hpp"
#include "chronicle/persist.hpp"
#include "chronicle/types.hpp"

namespace chronicle {

struct ToolRejection {
    std::string reason;
};

using ToolOutcome = std::expected<GameEvents, ToolRejection>;

class CartridgeGame {
  public:
    // Extra payload persisted alongside the world (NPC conversation history).
    using SnapshotHook = std::function<nlohmann::json()>;
    using RestoreHook = std::function<void(const nlohmann::json &)>;

    explicit CartridgeGame(const std::filesystem::path &package_dir,
                           std::optional<std::filesystem::path> save_dir = std::nullopt);
    CartridgeGame(WorldState world, std::optional<std::filesystem::path> save_dir = std::nullopt);

    [[nodiscard]] GamePhase phase() const { return phase_; }
    [[nodiscard]] const std::optional<std::string> &active_npc_id() const { return active_npc_; }
    [[nodiscard]] WorldState &world() { return world_; }
    [[nodiscard]] const WorldState &world() const { return world_; }

    [[nodiscard]] GameEvents bootstrap();
    [[nodiscard]] GameEvents handle_player(const std::string &text);
    [[nodiscard]] bool wants_llm_turn(const std::string &text) const;

    // The action gate: validate then apply one NPC tool call.
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
    [[nodiscard]] GameEvents apply_npc_tool(const std::string &npc_id,
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
    SaveSystem saves_;
    SnapshotHook conversation_snapshot_;
    RestoreHook conversation_restore_;
};

} // namespace chronicle
