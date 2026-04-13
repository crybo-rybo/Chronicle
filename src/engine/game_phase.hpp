/**
 * @file game_phase.hpp
 * @brief Enumeration of high-level game phases for Chronicle.
 *
 * @details The @ref GamePhase enum acts as a state-machine gate inside
 * @ref GameEngine.  The current phase determines which commands are valid,
 * how player input is classified by @ref CommandParser, and what post-turn
 * processing the engine performs.
 */

#pragma once

namespace chronicle {

/// @brief High-level phases that govern engine behaviour during a game session.
enum class GamePhase {
    Playing,        ///< Normal exploration: movement, inventory, and NPC interaction are active.
    InConversation, ///< The player is engaged in an active dialogue with an NPC.
                    ///  Most navigation commands are suspended; input is routed to the AI layer.
    Resolution,     ///< The final resolution event has fired; the engine is narrating the ending.
    GameOver        ///< The session has ended; only meta-commands (quit, restart) are accepted.
};

} // namespace chronicle
