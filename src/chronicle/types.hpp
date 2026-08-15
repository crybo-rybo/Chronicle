// Shared runtime types.
#pragma once

#include <string>
#include <vector>

namespace chronicle {

enum class GamePhase { playing, in_conversation, game_over };

[[nodiscard]] std::string to_string(GamePhase phase);
[[nodiscard]] GamePhase phase_from_string(const std::string &value);

enum class EventKind {
    narration,
    title,
    hint,
    look,
    system,
    dialogue,
    knowledge,
    tool_result,
    warning,
    ending,
};

// Something the renderer should show the player.
struct GameEvent {
    EventKind kind = EventKind::narration;
    std::string text;
};

using GameEvents = std::vector<GameEvent>;

inline std::string to_string(const GamePhase phase) {
    switch (phase) {
    case GamePhase::playing:
        return "playing";
    case GamePhase::in_conversation:
        return "in_conversation";
    case GamePhase::game_over:
        return "game_over";
    }
    return "playing";
}

inline GamePhase phase_from_string(const std::string &value) {
    if (value == "in_conversation") {
        return GamePhase::in_conversation;
    }
    if (value == "game_over") {
        return GamePhase::game_over;
    }
    return GamePhase::playing;
}

} // namespace chronicle
