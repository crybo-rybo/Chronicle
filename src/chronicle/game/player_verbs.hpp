// The words a player line can start with.
#pragma once

#include <algorithm>
#include <array>
#include <string_view>

namespace chronicle {

inline constexpr std::array<std::string_view, 13> PLAYER_VERBS{
    "go",        "look", "examine", "take", "drop", "use", "talk",
    "inventory", "save", "load",    "help", "quit", "q"};

// Whole lines that end a conversation.
inline constexpr std::array<std::string_view, 4> EXIT_PHRASES{"bye", "goodbye", "leave",
                                                              "exit conversation"};

[[nodiscard]] constexpr bool is_player_verb(const std::string_view word) {
    return std::ranges::find(PLAYER_VERBS, word) != PLAYER_VERBS.end();
}

[[nodiscard]] constexpr bool is_exit_phrase(const std::string_view line) {
    return std::ranges::find(EXIT_PHRASES, line) != EXIT_PHRASES.end();
}

} // namespace chronicle
