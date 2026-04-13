/**
 * @file terminal_renderer.hpp
 * @brief Plain-terminal @ref Renderer implementation for Chronicle.
 *
 * @details @ref TerminalRenderer renders all game output to a @c std::ostream
 * (defaulting to @c std::cout) using ANSI escape codes for colour when enabled.
 * Player input is read from a @c std::istream (defaulting to @c std::cin).
 *
 * ### Colour assignment
 * Each NPC receives a unique colour drawn from a fixed six-colour palette.
 * Colours are assigned lazily on first use, or eagerly via
 * @ref assign_npc_colors at construction time.  When a list of NPC names is
 * provided upfront, names are sorted alphabetically before assignment so
 * colour mapping is deterministic regardless of the order NPCs are encountered.
 *
 * ### Colour codes
 * The renderer uses standard ANSI SGR codes.  When @c use_color is @c false,
 * all escape sequences are suppressed and plain text is emitted.
 */

#pragma once
#include "rendering/renderer.hpp"
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace chronicle {

/// @brief ANSI-colour terminal renderer implementing the @ref Renderer interface.
///
/// @details Suitable for any VT-100 compatible terminal.  Inject @c std::ostringstream
/// instances for unit testing without coupling tests to @c stdout/@c stdin.
class TerminalRenderer : public Renderer {
  public:
    /// @brief Construct with default streams.
    ///
    /// @param out       Output stream.  Defaults to @c std::cout.
    /// @param in        Input stream.  Defaults to @c std::cin.
    /// @param use_color Enable ANSI colour output.  Defaults to @c true.
    explicit TerminalRenderer(std::ostream &out = std::cout, std::istream &in = std::cin,
                              bool use_color = true);

    /// @brief Construct with an upfront NPC name list for deterministic colour assignment.
    ///
    /// @details Names are sorted before colour assignment so that identical
    /// inputs always produce the same colour map.  Useful when the full NPC
    /// roster is known at startup.
    ///
    /// @param npc_names Names of all NPCs that may appear in the session.
    /// @param out       Output stream.
    /// @param in        Input stream.
    /// @param use_color Enable ANSI colour output.
    explicit TerminalRenderer(const std::vector<std::string> &npc_names,
                              std::ostream &out = std::cout, std::istream &in = std::cin,
                              bool use_color = true);

    void render_scene(const Location &loc, const World &world) override;
    void render_move(const std::string &direction, const std::string &new_location_name) override;
    void render_npc_intro(std::string_view npc_name, std::string_view mood) override;
    void begin_npc_dialogue(std::string_view npc_name) override;
    void stream_token(std::string_view token) override;
    void flush_dialogue() override;
    void render_action(std::string_view narration) override;
    void render_inventory(const Player &player, const World &world) override;
    void render_item_examine(const Item &item) override;
    void render_error(std::string_view message) override;
    void render_system(std::string_view message) override;
    void render_time_advance(const Clock &clock) override;
    void render_resolution(std::string_view ending_narration) override;
    std::string get_player_input(std::string_view prompt) override;
    void clear_input_line() override;

    /// @brief Pre-assign palette colours to a set of NPC names.
    ///
    /// @details NPC names are sorted alphabetically before assignment so the
    /// mapping is deterministic regardless of the order elements appear in
    /// @p npc_names.  Lazy assignment via @c npc_color() still works for any
    /// NPC not in the initial list.
    ///
    /// @param npc_names Names to assign colours to.
    void assign_npc_colors(const std::vector<std::string> &npc_names);

  private:
    std::ostream &out_; ///< Target output stream.
    std::istream &in_;  ///< Source input stream.
    bool use_color_;    ///< Whether ANSI escape codes are emitted.
    std::unordered_map<std::string, int> npc_color_indices_; ///< NPC name → palette index.
    int next_color_index_ = 0; ///< Next available palette index.

    // ANSI SGR escape codes (compile-time constants).
    static constexpr std::string_view kReset      = "\033[0m";
    static constexpr std::string_view kBold       = "\033[1m";
    static constexpr std::string_view kItalic     = "\033[3m";
    static constexpr std::string_view kDim        = "\033[2m";
    static constexpr std::string_view kBoldCyan   = "\033[1;36m";
    static constexpr std::string_view kSoftYellow = "\033[33m";

    /// @brief Six-colour NPC palette in bold ANSI colours.
    static constexpr std::array<std::string_view, 6> kNpcPalette = {{
        "\033[1;33m", ///< Bold yellow
        "\033[1;35m", ///< Bold magenta
        "\033[1;36m", ///< Bold cyan
        "\033[1;32m", ///< Bold green
        "\033[1;34m", ///< Bold blue
        "\033[1;31m", ///< Bold red
    }};

    /// @brief Return (and lazily assign) the palette colour for an NPC name.
    ///
    /// @param npc_name The NPC's display name.
    /// @return The ANSI escape sequence for this NPC's assigned colour.
    std::string_view npc_color(const std::string &npc_name);

    /// @brief Write @p text wrapped in an ANSI colour code, or plain if colour is disabled.
    ///
    /// @param color The ANSI escape sequence to apply.
    /// @param text  The text to write.
    void write_colored(std::string_view color, std::string_view text);
};

} // namespace chronicle
