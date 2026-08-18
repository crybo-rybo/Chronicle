#include "chronicle/runtime.hpp"

#include <algorithm>

namespace chronicle {

ConsoleRuntime::ConsoleRuntime(CartridgeGame &game, std::optional<EndpointConfig> endpoint)
    : ConsoleRuntime(game, std::move(endpoint), TerminalRenderer{}) {}

ConsoleRuntime::ConsoleRuntime(CartridgeGame &game, std::optional<EndpointConfig> endpoint,
                               TerminalRenderer renderer)
    : game_(game), renderer_(renderer) {
    if (endpoint && endpoint->configured()) {
        sessions_ = std::make_unique<NpcSessionManager>(game_, std::move(*endpoint));
        game_.set_conversation_hooks(
            [this] { return sessions_->snapshot_conversations(); },
            [this](const nlohmann::json &docs) { sessions_->restore_conversations(docs); });
    }
}

int ConsoleRuntime::run() {
    renderer_.print_events(game_.bootstrap());
    while (game_.phase() != GamePhase::game_over) {
        const std::string line = renderer_.prompt();
        renderer_.print_events(handle_line(line));
    }
    return 0;
}

GameEvents ConsoleRuntime::handle_line(const std::string &line) {
    GameEvents events;
    const auto append = [&events](GameEvents more) {
        events.insert(events.end(), std::make_move_iterator(more.begin()),
                      std::make_move_iterator(more.end()));
    };

    if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
        return events;
    }

    auto dispatch = game_.dispatch_player(line);
    append(std::move(dispatch.events));
    if (game_.phase() == GamePhase::game_over) {
        return events;
    }
    if (dispatch.npc_turn) {
        append(run_llm_turn(*dispatch.npc_turn));
    }
    // Significant turns (movement, items, successful dialogue tools) advance
    // the clock and may fire scripted events.
    append(game_.after_turn());
    return events;
}

GameEvents ConsoleRuntime::run_llm_turn(const NpcTurnRequest &request) {
    if (sessions_ != nullptr) {
        auto turn = sessions_->run_turn(request.npc_id, request.player_text);
        if (turn) {
            return std::move(*turn);
        }
        auto events = run_stub_turn(request.npc_id);
        const std::string state = turn.error().world_rolled_back
                                      ? "the world remained unchanged"
                                      : "world rollback failed; state may be inconsistent";
        events.insert(events.begin(), {EventKind::warning,
                                       "Inference failed (" + turn.error().message +
                                           "). Deterministic dialogue was used; " + state + "."});
        return events;
    }
    return run_stub_turn(request.npc_id);
}

GameEvents ConsoleRuntime::run_stub_turn(const std::string &npc_id) {
    const auto &npc = game_.world().npcs.at(npc_id);
    const auto &allowed = npc.identity.tool_policy.allowed_tools;
    if (std::ranges::find(allowed, std::string("say")) != allowed.end()) {
        auto outcome = game_.submit_npc_tool(npc_id, tools::Say{.text = STUB_REPLY});
        if (outcome) {
            return std::move(*outcome);
        }
    }
    // Fall back to plain dialogue if say is unavailable.
    return {{EventKind::dialogue, npc.identity.name + ": \"" + STUB_REPLY + "\""}};
}

} // namespace chronicle
