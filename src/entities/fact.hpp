/**
 * @file fact.hpp
 * @brief Authored knowledge records used by NPCs, prompts, and saves.
 *
 * @details Facts are scenario-authored pieces of world knowledge.  NPC
 * identity data stores fact IDs in its knowledge list; @ref PromptBuilder
 * resolves those IDs through @ref World::facts when constructing prompts, and
 * the @c reveal_knowledge mutation copies fact IDs into the player's known
 * facts.  Facts are data only and never mutate at runtime.
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

/// @brief A single author-declared fact in a scenario package.
///
/// @details Loaded from @c facts.json.  The @c id field is injected from the
/// JSON map key by @ref load_world, while @c text, @c category, and
/// @c revealed_by_default come from the fact object body.  The current runtime
/// stores all facts in @ref World::facts and uses player/NPC ID lists to track
/// who knows which fact.
struct Fact {
    /// Unique fact ID, injected from the @c facts.json map key.
    std::string id;

    /// Author-written fact text shown to the model when an NPC knows this fact.
    std::string text;

    /// Creator-defined grouping label, such as @c "backstory" or @c "clue".
    std::string category;

    /// @details The runtime preserves this schema field and, during world load,
    /// copies default-revealed facts into @ref Player::known_facts.
    bool revealed_by_default = false;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const Fact &f);
void from_json(const nlohmann::json &j, Fact &f);
/// @endcond

} // namespace chronicle
