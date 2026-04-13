#pragma once

#include "ai/npc_agent_pool.hpp"
#include "ai/response_handler.hpp"
#include "ai/tool_registry.hpp"
#include "engine/command_parser.hpp"
#include "engine/game_phase.hpp"
#include "engine/mutations.hpp"
#include "entities/config.hpp"
#include "entities/world.hpp"
#include "persistence/save_system.hpp"
#include "rendering/renderer.hpp"

#include <memory>
#include <optional>
#include <string>

namespace chronicle {

class GameEngine {
  public:
    // Initializes the engine. Expects the paths to config.json and data directory.
    // If renderer is null, instantiates TerminalRenderer by default.
    GameEngine(const std::string &config_path, const std::string &data_dir,
               std::unique_ptr<Renderer> renderer,
               std::unique_ptr<NpcAgentPool> agent_pool = nullptr);

    // Blocking: runs until the player quits or the game ends
    void run();

    // Exposed for testing
    void handle_command(const ParsedCommand &cmd);
    void process_pending_mutations();
    const World &world() const { return world_; }
    GamePhase phase() const { return phase_; }
    
    // Handles a dialogue turn with the active NPC agent, including tool mutations.
    void handle_dialogue(const std::string &npc_id, const std::string &input);

  private:
    Config config_;
    World world_;
    CommandParser parser_;
    std::unique_ptr<Renderer> renderer_;
    SaveSystem save_system_;
    TokenQueue token_queue_;
    
    // AI subsystems (pointers to allow delayed or optional instantiation)
    std::unique_ptr<NpcAgentPool> agent_pool_;
    std::unique_ptr<ToolRegistry> tool_registry_;
    std::unique_ptr<ResponseHandler> response_handler_;

    bool running_ = true;
    GamePhase phase_ = GamePhase::Playing;
    std::optional<std::string> current_conversation_npc_id_;

    void render_current_scene();
    void render_inventory();
    bool drain_token_queue();

    // Helper to find exits/items
    bool player_can_see_item(const std::string &item_id) const;
    bool player_can_see_npc(const std::string &npc_id) const;
    std::optional<std::string> find_visible_npc_id(const std::string &query) const;
    std::optional<std::string> find_accessible_item_id(const std::string &query) const;
    std::optional<int> parse_slot(const std::string &slot_text) const;
    void leave_conversation();
    bool is_conversation_exit(std::string_view input) const;
};

} // namespace chronicle
