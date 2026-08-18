// Typed domain actions and the single runtime mutation boundary.
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <variant>

#include "chronicle/cartridge/models.hpp"
#include "chronicle/types.hpp"

namespace chronicle::actions {

struct MovePlayer {
    std::string direction;
};

struct RelocateItem {
    std::string item_id;
    ItemPosition destination;
    bool significant = true;
};

struct UnlockExit {
    std::string location_id;
    std::string direction;
};

struct BeginConversation {
    std::string npc_id;
};

struct EndConversation {};
struct EndGame {};

struct AdvanceClock {
    int turns = 1;
};

struct MarkEventFired {
    std::string event_id;
};

struct MoveNpc {
    std::string npc_id;
    std::string destination;
    bool significant = false;
};

struct UpdateNpcMood {
    std::string npc_id;
    std::string mood;
};

struct AdjustNpcTrust {
    std::string npc_id;
    int delta = 0;
};

struct RevealFact {
    std::string fact_id;
};

struct AddMemory {
    std::string npc_id;
    MemoryEntry memory;
};

struct SetFlag {
    std::string flag_id;
    bool value = false;
    bool significant = true;
};

struct RestoreRuntime {
    WorldState world;
    GamePhase phase = GamePhase::playing;
    std::optional<std::string> active_npc;
};

using WorldAction =
    std::variant<MovePlayer, RelocateItem, UnlockExit, BeginConversation, EndConversation, EndGame,
                 AdvanceClock, MarkEventFired, MoveNpc, UpdateNpcMood, AdjustNpcTrust, RevealFact,
                 AddMemory, SetFlag, RestoreRuntime>;

struct ActionRejection {
    std::string reason;
};

using ActionOutcome = std::expected<void, ActionRejection>;

class ActionGate {
  public:
    ActionGate(WorldState &world, GamePhase &phase, std::optional<std::string> &active_npc,
               bool &significant);

    [[nodiscard]] ActionOutcome submit(WorldAction action);

  private:
    WorldState &world_;
    GamePhase &phase_;
    std::optional<std::string> &active_npc_;
    bool &significant_;
};

} // namespace chronicle::actions
