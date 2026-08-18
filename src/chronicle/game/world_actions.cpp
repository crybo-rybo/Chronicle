#include "chronicle/game/world_actions.hpp"

#include <algorithm>
#include <limits>
#include <ranges>

#include "chronicle/cartridge/validator.hpp"

namespace chronicle::actions {

namespace {

ActionOutcome reject(std::string reason) {
    return std::unexpected(ActionRejection{.reason = std::move(reason)});
}

bool nonblank(const std::string &text) {
    return text.find_first_not_of(" \t\r\n") != std::string::npos;
}

} // namespace

ActionGate::ActionGate(WorldState &world, GamePhase &phase,
                       std::optional<std::string> &active_npc, bool &significant)
    : world_(world), phase_(phase), active_npc_(active_npc), significant_(significant) {}

ActionOutcome ActionGate::submit(WorldAction action) {
    if (phase_ == GamePhase::game_over && !std::holds_alternative<EndGame>(action) &&
        !std::holds_alternative<RestoreRuntime>(action)) {
        return reject("The game has ended");
    }

    struct Visitor {
        ActionGate &gate;

        ActionOutcome operator()(const MovePlayer &move) const {
            const auto location = gate.world_.locations.find(gate.world_.player.current_location);
            if (location == gate.world_.locations.end()) {
                return reject("Unknown player location");
            }
            const auto exit = location->second.exits.find(move.direction);
            if (exit == location->second.exits.end()) {
                return reject("Unknown player exit");
            }
            if (location->second.locked_directions().contains(move.direction)) {
                return reject("Player exit is locked");
            }
            gate.world_.player.current_location = exit->second;
            gate.significant_ = true;
            return {};
        }

        ActionOutcome operator()(const RelocateItem &relocate) const {
            if (!gate.world_.items.contains(relocate.item_id)) {
                return reject("Unknown item");
            }
            if (relocate.destination.holder == ItemHolder::location &&
                !gate.world_.locations.contains(relocate.destination.id)) {
                return reject("Unknown item destination");
            }
            if (relocate.destination.holder == ItemHolder::npc &&
                !gate.world_.npcs.contains(relocate.destination.id)) {
                return reject("Unknown item recipient");
            }
            if (relocate.destination.holder == ItemHolder::player &&
                !relocate.destination.id.empty()) {
                return reject("Player item position must not carry an id");
            }
            gate.world_.item_positions[relocate.item_id] = relocate.destination;
            gate.significant_ = gate.significant_ || relocate.significant;
            return {};
        }

        ActionOutcome operator()(const UnlockExit &unlock) const {
            const auto location = gate.world_.locations.find(unlock.location_id);
            if (location == gate.world_.locations.end() ||
                !location->second.exits.contains(unlock.direction)) {
                return reject("Unknown exit");
            }
            const auto removed = std::erase_if(location->second.locked_exits,
                                               [&](const LockedExitEntry &entry) {
                                                   return entry.direction == unlock.direction;
                                               });
            if (removed == 0) {
                return reject("Exit is not locked");
            }
            gate.significant_ = true;
            return {};
        }

        ActionOutcome operator()(const BeginConversation &begin) const {
            const auto npc = gate.world_.npcs.find(begin.npc_id);
            if (npc == gate.world_.npcs.end()) {
                return reject("Unknown NPC");
            }
            if (gate.phase_ == GamePhase::game_over ||
                npc->second.state.current_location != gate.world_.player.current_location) {
                return reject("NPC is not available for conversation");
            }
            gate.phase_ = GamePhase::in_conversation;
            gate.active_npc_ = begin.npc_id;
            npc->second.state.has_met_player = true;
            return {};
        }

        ActionOutcome operator()(const EndConversation &) const {
            if (gate.phase_ != GamePhase::in_conversation) {
                return reject("No conversation is active");
            }
            gate.phase_ = GamePhase::playing;
            gate.active_npc_.reset();
            return {};
        }

        ActionOutcome operator()(const EndGame &) const {
            gate.phase_ = GamePhase::game_over;
            gate.active_npc_.reset();
            gate.significant_ = false;
            return {};
        }

        ActionOutcome operator()(const AdvanceClock &advance) const {
            if (advance.turns < 0 ||
                gate.world_.clock.turns_elapsed >
                    std::numeric_limits<int>::max() - advance.turns) {
                return reject("Invalid clock advance");
            }
            gate.world_.clock.turns_elapsed += advance.turns;
            gate.significant_ = false;
            return {};
        }

        ActionOutcome operator()(const MarkEventFired &mark) const {
            const auto event = gate.world_.events.find(mark.event_id);
            if (event == gate.world_.events.end()) {
                return reject("Unknown event");
            }
            event->second.fired = true;
            return {};
        }

        ActionOutcome operator()(const MoveNpc &move) const {
            const auto npc = gate.world_.npcs.find(move.npc_id);
            if (npc == gate.world_.npcs.end() || !gate.world_.locations.contains(move.destination)) {
                return reject("Unknown NPC destination");
            }
            npc->second.state.current_location = move.destination;
            gate.significant_ = gate.significant_ || move.significant;
            if (gate.active_npc_ == move.npc_id &&
                move.destination != gate.world_.player.current_location) {
                gate.phase_ = GamePhase::playing;
                gate.active_npc_.reset();
            }
            return {};
        }

        ActionOutcome operator()(const UpdateNpcMood &update) const {
            const auto npc = gate.world_.npcs.find(update.npc_id);
            if (npc == gate.world_.npcs.end() ||
                std::ranges::find(valid_moods(), update.mood) == valid_moods().end()) {
                return reject("Invalid NPC mood update");
            }
            npc->second.state.mood = update.mood;
            return {};
        }

        ActionOutcome operator()(const AdjustNpcTrust &adjust) const {
            const auto npc = gate.world_.npcs.find(adjust.npc_id);
            if (npc == gate.world_.npcs.end()) {
                return reject("Unknown NPC");
            }
            const auto widened = static_cast<long long>(npc->second.state.trust_toward_player) +
                                 static_cast<long long>(adjust.delta);
            npc->second.state.trust_toward_player =
                static_cast<int>(std::clamp(widened, 0LL, 100LL));
            if (!npc->second.identity.secret.empty() &&
                npc->second.state.trust_toward_player >=
                    npc->second.identity.trust_reveal_threshold) {
                npc->second.state.secret_revealed = true;
            }
            return {};
        }

        ActionOutcome operator()(const RevealFact &reveal) const {
            if (!gate.world_.facts.contains(reveal.fact_id)) {
                return reject("Unknown fact");
            }
            gate.world_.revealed_facts.insert(reveal.fact_id);
            gate.significant_ = true;
            return {};
        }

        ActionOutcome operator()(AddMemory &add) const {
            const auto npc = gate.world_.npcs.find(add.npc_id);
            if (npc == gate.world_.npcs.end() || !nonblank(add.memory.summary) ||
                add.memory.importance < 1 || add.memory.importance > 10) {
                return reject("Invalid NPC memory");
            }
            npc->second.state.memories.push_back(std::move(add.memory));
            return {};
        }

        ActionOutcome operator()(const SetFlag &set) const {
            if (!gate.world_.flags.contains(set.flag_id) &&
                !gate.world_.flag_meta.contains(set.flag_id)) {
                return reject("Unknown flag");
            }
            gate.world_.flags[set.flag_id] = set.value;
            gate.significant_ = gate.significant_ || set.significant;
            return {};
        }

        ActionOutcome operator()(RestoreRuntime &restore) const {
            if (has_errors(validate_world(restore.world))) {
                return reject("Saved world failed validation");
            }
            if (restore.phase == GamePhase::in_conversation) {
                if (!restore.active_npc || !restore.world.npcs.contains(*restore.active_npc) ||
                    restore.world.npcs.at(*restore.active_npc).state.current_location !=
                        restore.world.player.current_location) {
                    return reject("Saved conversation state is inconsistent");
                }
            } else if (restore.active_npc) {
                return reject("Saved active NPC does not match the game phase");
            }
            gate.world_ = std::move(restore.world);
            gate.phase_ = restore.phase;
            gate.active_npc_ = std::move(restore.active_npc);
            gate.significant_ = false;
            return {};
        }
    };

    return std::visit(Visitor{*this}, action);
}

} // namespace chronicle::actions
