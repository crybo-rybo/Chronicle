/**
 * @file mutation_checks.hpp
 * @brief Non-mutating preconditions shared by validators and apply functions.
 */

#pragma once
#include "engine/mutation_request.hpp"
#include "entities/world.hpp"
#include <optional>
#include <string>

namespace chronicle {

/// @return Error message when the mutation cannot be applied; @c std::nullopt if OK.
std::optional<std::string> check_mutation(const World &world, const MutationRequest &mutation);

} // namespace chronicle
