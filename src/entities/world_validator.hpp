/**
 * @file world_validator.hpp
 * @brief Structural integrity validator for the World state.
 *
 * @details Provides a single entry point, @ref validate_world, that checks
 * cross-entity referential integrity (dangling IDs, duplicate ownership, etc.)
 * and returns a human-readable report of errors and warnings.
 *
 * Event trigger conditions and action parameters are also checked so authored
 * scenario data fails fast before gameplay starts.
 */

#pragma once
#include "entities/world.hpp"
#include <string>
#include <vector>

namespace chronicle {

/// @brief Result of a structural validation pass over a @ref World instance.
///
/// @details If @c ok is @c true, no errors were found (warnings may still be
/// present).  Each string in @c errors or @c warnings is a self-contained,
/// human-readable description of a single issue.  Errors block scenario
/// execution; warnings are suspicious-but-valid authoring guidance.
struct ValidationReport {
    bool ok = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// @brief Validate the structural integrity of a World.
///
/// @details Pure inspection pass: the validator never mutates @p world and
/// never requires a model.  Checks include:
/// - All entity cross-references (location exits, NPC locations, item
///   ownership, player location/inventory) point to entities that exist.
/// - Item ownership uniqueness (each item appears in at most one container).
/// - NPC knowledge fact IDs exist in the world facts registry.
/// - Event trigger/action parameters refer only to declared flags and entities.
/// - Warning-level checks for items with inconsistent properties.
///
/// This validates scenario authoring constraints, not save-file compatibility.
///
/// @param world The world state to validate.
/// @return A @ref ValidationReport summarising all issues found.
ValidationReport validate_world(const World &world);

} // namespace chronicle
