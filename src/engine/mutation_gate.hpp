/**
 * @file mutation_gate.hpp
 * @brief Pure-function validators that produce player MutationRequest values.
 *
 * @details Each @c validate_player_* function inspects the current @ref World
 * and returns either a valid @ref MutationRequest or an error string.  These
 * are the player-command analogues of @ref ToolRegistry's NPC validation
 * methods.
 */

#pragma once
#include "engine/mutation_request.hpp"
#include "entities/world.hpp"
#include <string>
#include <string_view>
#include <variant>

namespace chronicle {

/// @brief Validate a player movement command.
/// @return A PlayerMove MutationRequest or an error string.
std::variant<MutationRequest, std::string> validate_player_move(const World &world,
                                                                 std::string_view direction);

/// @brief Validate a player take command.
/// @return A PlayerTake MutationRequest or an error string.
std::variant<MutationRequest, std::string> validate_player_take(const World &world,
                                                                 std::string_view item_query);

/// @brief Validate a player drop command.
/// @return A PlayerDrop MutationRequest or an error string.
std::variant<MutationRequest, std::string> validate_player_drop(const World &world,
                                                                 std::string_view item_query);

} // namespace chronicle
