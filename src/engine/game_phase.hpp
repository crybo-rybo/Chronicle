#pragma once

namespace chronicle {

enum class GamePhase {
    Playing,        // Normal exploration and interaction
    InConversation, // Mid-conversation with an NPC
    Resolution,     // Final ending narration
    GameOver        // Done, awaiting quit/restart
};

} // namespace chronicle
