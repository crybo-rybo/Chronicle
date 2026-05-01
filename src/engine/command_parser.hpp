/**
 * @file command_parser.hpp
 * @brief Player input parsing for Chronicle.
 *
 * @details @ref CommandParser converts raw text entered by the player into a
 * structured @ref ParsedCommand value.  It is a pure function mapping strings
 * to commands — it never reads or writes world state.
 *
 * The parser supports two modes determined by the current @ref GamePhase:
 *
 * - **Playing** — all recognised verbs are dispatched normally.
 * - **InConversation** — only a fixed set of "hard" commands (quit, save,
 *   load, help, inventory, look) break out of dialogue; any other input is
 *   classified as @ref CommandVerb::Dialogue and forwarded verbatim to the
 *   AI layer.
 *
 * Verb aliases (e.g. @c "n" → @c "go north", @c "i" → @c "inventory") are
 * loaded from the @c verb_aliases section of the config JSON file.  If the
 * file is absent or lacks a @c verb_aliases key, a built-in fallback table is
 * used so the game remains playable without a config file.
 */

#pragma once
#include "engine/game_phase.hpp"
#include <filesystem>
#include <string>
#include <unordered_map>

namespace chronicle {

/// @brief Canonical set of actions the player can perform.
enum class CommandVerb {
    Go,        ///< Move in a direction.  @c primary_arg is the direction string.
    Look,      ///< Re-display the current location description.
    Examine,   ///< Inspect an item.  @c primary_arg is the item name or ID.
    Take,      ///< Pick up an item from the current location.
    Drop,      ///< Drop an item from the player's inventory.
    Use,       ///< Use an item, optionally targeting another object.
    Give,      ///< Give an item to an NPC (reserved; routing TBD).
    Talk,      ///< Begin a conversation with an NPC.  @c primary_arg is the NPC name or ID.
    Inventory, ///< Display the player's current inventory.
    Save,      ///< Save the game to a slot.  @c primary_arg is the slot number.
    Load,      ///< Load a saved game from a slot.  @c primary_arg is the slot number.
    Quit,      ///< Exit the game.
    Help,      ///< Display help text.
    Dialogue,  ///< Raw NPC dialogue input.  @c primary_arg holds the full player text.
    Unknown    ///< Input that could not be matched to any known verb.
};

/// @brief The result of parsing a single line of player input.
///
/// @details All fields are populated by @ref CommandParser::parse.  The meaning
/// of @c primary_arg and @c secondary_arg depends on the verb:
///
/// | Verb       | primary_arg          | secondary_arg      |
/// |------------|----------------------|--------------------|
/// | Go         | direction            | —                  |
/// | Examine    | item name / ID       | —                  |
/// | Take / Drop| item name            | —                  |
/// | Use        | item name            | target name / ID   |
/// | Talk       | NPC name / ID        | —                  |
/// | Save / Load| slot number string   | —                  |
/// | Dialogue   | full player utterance| —                  |
struct ParsedCommand {
    CommandVerb verb = CommandVerb::Unknown; ///< Resolved action type.
    std::string primary_arg;                 ///< First argument extracted from the input.
    std::string secondary_arg;               ///< Second argument (e.g., target of @c use).
    std::string raw_input;                   ///< The original unmodified input string.
};

/// @brief Converts raw player input into a @ref ParsedCommand.
///
/// @details The parser is stateless and phase-aware.  It supports an
/// extensible alias table loaded from a JSON config file.  The @c "use" verb
/// receives special handling to extract @c "item on target" syntax.
class CommandParser {
  public:
    /// @brief Construct with an explicit path to the config JSON file.
    ///
    /// @param config_path Path to the config file containing a @c verb_aliases
    ///        JSON object.  Falls back to the built-in table if the key is absent.
    /// @throws std::runtime_error if the file exists but contains invalid JSON.
    explicit CommandParser(std::filesystem::path config_path);

    /// @brief Parse a single line of player input.
    ///
    /// @details Trims and lower-cases the first word, looks it up in the verb
    /// table, and extracts arguments.  During @ref GamePhase::InConversation,
    /// only hard commands are resolved; everything else becomes
    /// @ref CommandVerb::Dialogue.
    ///
    /// @param raw_input The unprocessed string from the player.
    /// @param phase     The current game phase, used to gate dialogue passthrough.
    /// @return A @ref ParsedCommand with the resolved verb and extracted arguments.
    ParsedCommand parse(const std::string &raw_input, GamePhase phase) const;

  private:
    /// Verb lookup table: lowercase alias → @ref CommandVerb.
    std::unordered_map<std::string, CommandVerb> verb_table_;

    /// @brief Parse the argument string following a @c "use" verb.
    ///
    /// @details Splits the arguments on @c " on " or @c " with " to extract
    /// the primary item and the optional target object.
    ///
    /// @param args_after_verb Everything after the initial "use" token.
    /// @param raw              The full original input, preserved in the result.
    /// @return A @ref ParsedCommand with @c verb == @ref CommandVerb::Use.
    ParsedCommand parse_use_syntax(const std::string &args_after_verb,
                                   const std::string &raw) const;
};

} // namespace chronicle
