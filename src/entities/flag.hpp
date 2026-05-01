/**
 * @file flag.hpp
 * @brief Scenario-declared boolean narrative flags.
 *
 * @details Flags are author-defined milestones used by NPC tools, event
 * conditions, and resolution logic.  @c flags.json declares the available flag
 * IDs and their default values; runtime state stores the current boolean value
 * in @ref World::flags.
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

/// @brief Declaration of a single scenario flag.
///
/// @details Loaded from @c flags.json.  The @c id field is injected from the
/// JSON map key by @ref load_world.  After loading, only the boolean value is
/// stored in @ref World::flags; descriptions remain authoring metadata and
/// validation context.
struct Flag {
    /// Unique flag ID, injected from the @c flags.json map key.
    std::string id;

    /// Initial value assigned to @ref World::flags at scenario load time.
    bool default_value = false;

    /// Human-readable authoring note explaining what the flag represents.
    std::string description;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const Flag &f);
void from_json(const nlohmann::json &j, Flag &f);
/// @endcond

} // namespace chronicle
