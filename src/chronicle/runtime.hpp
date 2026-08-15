// Console runtime: turn loop, LLM sessions, stub degrade.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "chronicle/game/cartridge_game.hpp"
#include "chronicle/llm/npc_sessions.hpp"
#include "chronicle/render.hpp"
#include "chronicle/types.hpp"

namespace chronicle {

// Deterministic no-network reply used when no model endpoint is configured.
inline constexpr const char *STUB_REPLY = "The figure regards you quietly.";

class ConsoleRuntime {
  public:
    // A nullopt endpoint selects the stub: mechanics, events, and save/load
    // keep working with no model at all.
    ConsoleRuntime(CartridgeGame &game, std::optional<EndpointConfig> endpoint);
    ConsoleRuntime(CartridgeGame &game, std::optional<EndpointConfig> endpoint,
                   TerminalRenderer renderer);

    // Blocking play loop; returns the process exit code.
    int run();

    // One player input line -> events (also used directly by tests).
    [[nodiscard]] GameEvents handle_line(const std::string &line);

    [[nodiscard]] bool using_stub() const { return sessions_ == nullptr; }

  private:
    [[nodiscard]] GameEvents run_llm_turn(const std::string &text);
    [[nodiscard]] GameEvents run_stub_turn();

    CartridgeGame &game_;
    TerminalRenderer renderer_;
    std::unique_ptr<NpcSessionManager> sessions_;
};

} // namespace chronicle
