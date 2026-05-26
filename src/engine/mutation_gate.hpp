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
///
/// @details Checks that the player is in a known location, that @p direction
/// is an exact exit key from that location, and that the exit is not currently
/// locked.
///
/// @param world     Current world state to inspect without mutation.
/// @param direction Exit direction supplied by the parser.
/// @return A @ref MutationRequest of type @c PlayerMove with @c location_id
///         and @c direction params, or a player-facing error string.
std::variant<MutationRequest, std::string> validate_player_move(const World &world,
                                                                std::string_view direction);

/// @brief Validate a player take command.
///
/// @details Searches visible items in the player's current location by
/// case-insensitive partial name or ID match.  Hidden and non-takeable items
/// are ignored so they cannot be acquired through a normal @c take command.
///
/// @param world      Current world state to inspect without mutation.
/// @param item_query Player-supplied item name or ID fragment.
/// @return A @ref MutationRequest of type @c PlayerTake with @c item_id, or a
///         player-facing error string.
std::variant<MutationRequest, std::string> validate_player_take(const World &world,
                                                                std::string_view item_query);

/// @brief Validate a player drop command.
///
/// @details Searches the player's inventory by case-insensitive partial name
/// or ID match.  Key items are rejected to avoid making authored scenarios
/// unwinnable by leaving critical items behind.
///
/// @param world      Current world state to inspect without mutation.
/// @param item_query Player-supplied item name or ID fragment.
/// @return A @ref MutationRequest of type @c PlayerDrop with @c item_id, or a
///         player-facing error string.
std::variant<MutationRequest, std::string> validate_player_drop(const World &world,
                                                                std::string_view item_query);

/// @brief Validate a player unlock-exit use command.
///
/// @details Requires a carried item whose @c unlock_target matches the locked
/// exit destination resolved from @p target_query.
std::variant<MutationRequest, std::string>
validate_unlock_exit(const World &world, std::string_view item_query, std::string_view target_query);

} // namespace chronicle
