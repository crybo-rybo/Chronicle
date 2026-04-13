/**
 * @file renderer.hpp
 * @brief Abstract rendering interface for Chronicle.
 *
 * @details @ref Renderer decouples all output and input from the game logic.
 * The engine calls renderer methods to present information to the player and
 * to collect player input, without knowing or caring whether the output goes
 * to a plain @c std::cout terminal, an ANSI-colour terminal, or a future TUI.
 *
 * The interface is deliberately narrow: one method per distinct visual concept
 * (scene description, movement, dialogue streaming, inventory, etc.).  Concrete
 * implementations may render these differently but must honour the semantic
 * contract described for each method.
 *
 * The concrete MVP implementation is @ref TerminalRenderer.
 */

#pragma once
#include "entities/clock.hpp"
#include "entities/item.hpp"
#include "entities/location.hpp"
#include "entities/player.hpp"
#include "entities/world.hpp"
#include <string>
#include <string_view>

namespace chronicle {

/// @brief Abstract output and input interface.
///
/// @details All methods except @ref get_player_input produce output only;
/// they must not block.  @ref get_player_input blocks until the player
/// presses Enter.
class Renderer {
  public:
    /// @brief Render the full scene description for a location.
    ///
    /// @details Displays the location name, its description text, available
    /// exits, visible items, and NPCs currently present.
    ///
    /// @param loc   The location to describe.
    /// @param world The current world state, used to resolve item and NPC names.
    virtual void render_scene(const Location &loc, const World &world) = 0;

    /// @brief Narrate the player moving through an exit.
    ///
    /// @param direction       The direction the player moved (e.g. "north").
    /// @param new_location_name The display name of the destination location.
    virtual void render_move(const std::string &direction,
                             const std::string &new_location_name) = 0;

    /// @brief Introduce an NPC with a brief presence note.
    ///
    /// @param npc_name The NPC's display name.
    /// @param mood     The NPC's current mood string.
    virtual void render_npc_intro(std::string_view npc_name, std::string_view mood) = 0;

    /// @brief Signal the start of NPC streaming dialogue.
    ///
    /// @details Called once before the first @ref stream_token call for a
    /// given NPC turn.  Implementations may use this to set colour, print a
    /// name prefix, or otherwise prepare the display.
    ///
    /// @param npc_name The NPC's display name.
    virtual void begin_npc_dialogue(std::string_view npc_name) = 0;

    /// @brief Output a single streaming token of NPC dialogue.
    ///
    /// @details Called for each token drained from the @ref TokenQueue.
    /// Should flush immediately so the player sees tokens as they arrive.
    ///
    /// @param token The token text to display.
    virtual void stream_token(std::string_view token) = 0;

    /// @brief Signal the end of a streaming dialogue turn.
    ///
    /// @details Called after all tokens for an NPC turn have been displayed.
    /// Implementations should reset any colour state set by @ref begin_npc_dialogue
    /// and print a trailing newline.
    virtual void flush_dialogue() = 0;

    /// @brief Render a narrated action (e.g. an NPC handing over an item).
    ///
    /// @details Action narration is visually distinct from dialogue (typically
    /// italic or surrounded by asterisks).
    ///
    /// @param narration The action description to display.
    virtual void render_action(std::string_view narration) = 0;

    /// @brief Render the player's current inventory.
    ///
    /// @param player The player state providing the inventory list.
    /// @param world  The world used to look up item display names.
    virtual void render_inventory(const Player &player, const World &world) = 0;

    /// @brief Display the detailed description of an item.
    ///
    /// @param item The item to describe.  May include readable text from properties.
    virtual void render_item_examine(const Item &item) = 0;

    /// @brief Display an error or "can't do that" message.
    ///
    /// @param message The error text to show.
    virtual void render_error(std::string_view message) = 0;

    /// @brief Display a system/meta message (save confirmation, help text, etc.).
    ///
    /// @param message The message to show.
    virtual void render_system(std::string_view message) = 0;

    /// @brief Announce a time-of-day transition.
    ///
    /// @param clock The updated clock state to display (e.g. "Afternoon, Day 2").
    virtual void render_time_advance(const Clock &clock) = 0;

    /// @brief Display the game's ending narration.
    ///
    /// @param ending_narration The full resolution text to display.
    virtual void render_resolution(std::string_view ending_narration) = 0;

    /// @brief Block and wait for a line of player input.
    ///
    /// @param prompt The prompt string to display before the input cursor
    ///               (e.g. @c "> ").
    /// @return The player's input with the trailing newline stripped.
    virtual std::string get_player_input(std::string_view prompt) = 0;

    /// @brief Clear the current input line (used to overwrite partial input).
    virtual void clear_input_line() = 0;

    virtual ~Renderer() = default;
};

} // namespace chronicle
