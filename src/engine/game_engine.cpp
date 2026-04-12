#include "engine/game_engine.hpp"
#include "ai/prompt_builder.hpp"
#include "ai/zoo_agent_adapter.hpp"
#include "entities/world_loader.hpp"
#include "rendering/terminal_renderer.hpp"
#include <algorithm>
#include <iostream>

namespace chronicle {

// Helper declaration locally for 'to_lower' which parser has internally. 
// For now, I will manually inline or declare to_lower here to avoid unreferenced link errors.
inline std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

GameEngine::GameEngine(const std::string &config_path, const std::string &data_dir,
                       std::unique_ptr<Renderer> renderer)
    : config_(Config::load(config_path)),
      world_(load_world(data_dir)),
      parser_(config_path),
      renderer_(std::move(renderer)),
      save_system_(config_.save_directory.empty() ? "saves" : config_.save_directory) {

    if (!renderer_) {
        renderer_ = std::make_unique<TerminalRenderer>();
    }

    tool_registry_ = std::make_unique<ToolRegistry>(world_);
    response_handler_ = std::make_unique<ResponseHandler>(*renderer_, token_queue_, world_);
    
    if (!config_.model_path.empty()) {
        try {
            agent_pool_ = std::make_unique<NpcAgentPool>(NpcAgentPool::from_config(config_));
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize Agent Pool: " << e.what() << "\n";
        }
    }
}

void GameEngine::run() {
    render_current_scene();
    
    while (running_) {
        // Stream any dialogue tokens
        while (auto token = token_queue_.try_pop()) {
            renderer_->stream_token(*token);
        }

        std::string prompt = "\n> ";
        if (phase_ == GamePhase::InConversation) {
            prompt = "(Conversation) > ";
        }
        
        std::string input = renderer_->get_player_input(prompt);
        if (input.empty()) {
            continue;
        }

        auto cmd = parser_.parse(input, phase_);
        handle_command(cmd);
        
        process_pending_mutations();
    }
}

void GameEngine::render_current_scene() {
    auto loc_it = world_.locations.find(world_.player.current_location);
    if (loc_it != world_.locations.end()) {
        renderer_->render_scene(loc_it->second, world_);
    }
}

void GameEngine::render_inventory() {
    renderer_->render_inventory(world_.player, world_);
}

void GameEngine::handle_command(const ParsedCommand &cmd) {
    if (cmd.verb == CommandVerb::Unknown) {
        renderer_->render_error("I don't understand that command.");
        return;
    }
    
    if (cmd.verb == CommandVerb::Dialogue) {
        handle_dialogue(cmd.primary_arg, cmd.raw_input); // Assuming primary_arg has target or raw_input handles it
        return;
    }

    if (cmd.verb == CommandVerb::Quit) {
        renderer_->render_system("Thanks for playing!");
        running_ = false;
        return;
    }

    if (cmd.verb == CommandVerb::Go) {
        auto dir = cmd.primary_arg;
        auto loc_it = world_.locations.find(world_.player.current_location);
        if (loc_it != world_.locations.end()) {
            auto exit_it = loc_it->second.exits.find(dir);
            if (exit_it != loc_it->second.exits.end()) {
                world_.player.current_location = exit_it->second;
                auto dest_it = world_.locations.find(exit_it->second);
                std::string name = dest_it != world_.locations.end() ? dest_it->second.name : exit_it->second;
                renderer_->render_move(dir, name);
                render_current_scene();
            } else {
                renderer_->render_error("You can't go that way.");
            }
        }
        return;
    }

    if (cmd.verb == CommandVerb::Look) {
        render_current_scene();
        return;
    }

    if (cmd.verb == CommandVerb::Inventory) {
        render_inventory();
        return;
    }

    if (cmd.verb == CommandVerb::Take) {
        std::string item_name = cmd.primary_arg;
        auto loc_it = world_.locations.find(world_.player.current_location);
        if (loc_it != world_.locations.end()) {
            auto &items = loc_it->second.items;
            // Search items in location
            auto item_it = std::ranges::find_if(items, [&](const std::string &id) {
                auto w_it = world_.items.find(id);
                if (w_it != world_.items.end()) {
                    return chronicle::to_lower(w_it->second.name).find(item_name) != std::string::npos ||
                           chronicle::to_lower(id).find(item_name) != std::string::npos;
                }
                return false;
            });
            if (item_it != items.end()) {
                world_.player.inventory.push_back(*item_it);
                auto w_it = world_.items.find(*item_it);
                renderer_->render_action("You take the " + (w_it != world_.items.end() ? w_it->second.name : *item_it) + ".");
                items.erase(item_it);
            } else {
                renderer_->render_error("You don't see that here.");
            }
        }
        return;
    }

    if (cmd.verb == CommandVerb::Drop) {
        std::string item_name = cmd.primary_arg;
        auto &inv = world_.player.inventory;
        auto item_it = std::ranges::find_if(inv, [&](const std::string &id) {
            auto w_it = world_.items.find(id);
            if (w_it != world_.items.end()) {
                return chronicle::to_lower(w_it->second.name).find(item_name) != std::string::npos ||
                       chronicle::to_lower(id).find(item_name) != std::string::npos;
            }
            return false;
        });
        if (item_it != inv.end()) {
            auto loc_it = world_.locations.find(world_.player.current_location);
            if (loc_it != world_.locations.end()) {
                loc_it->second.items.push_back(*item_it);
            }
            auto w_it = world_.items.find(*item_it);
            renderer_->render_action("You drop the " + (w_it != world_.items.end() ? w_it->second.name : *item_it) + ".");
            inv.erase(item_it);
        } else {
            renderer_->render_error("You aren't carrying that.");
        }
        return;
    }
    
    if (cmd.verb == CommandVerb::Talk) {
        // Find NPC locally
        std::string npc_name = cmd.primary_arg;
        auto loc_it = world_.locations.find(world_.player.current_location);
        if (loc_it != world_.locations.end()) {
            auto it = std::ranges::find_if(loc_it->second.npcs, [&](const std::string &id) {
                auto npc_it = world_.npcs.find(id);
                if (npc_it != world_.npcs.end()) {
                    return chronicle::to_lower(npc_it->second.identity.name).find(npc_name) != std::string::npos ||
                           chronicle::to_lower(id).find(npc_name) != std::string::npos;
                }
                return false;
            });
            if (it != loc_it->second.npcs.end()) {
                // Enter conversation
                phase_ = GamePhase::InConversation;
                renderer_->render_system("You are now talking to " + *it + ".");
                // For now we don't store "current conversational partner" permanently
                // since the next input will be routed here via Dialogue
                // Let's just output a generic system message
            } else {
                renderer_->render_error("There is no one here by that name.");
            }
        }
        return;
    }

    renderer_->render_error("That command is not fully implemented yet.");
}

void GameEngine::process_pending_mutations() {
    auto mutations = tool_registry_->pending_mutations();
    if (!mutations.empty()) {
        for (const auto &m : mutations) {
            apply_mutation(world_, m);
        }
        response_handler_->narrate_mutations(mutations, "NPC"); // The real NPC name will be passed in integration
        tool_registry_->clear_pending();
    }
}

void GameEngine::handle_dialogue(const std::string &npc_id, const std::string &input) {
    if (!agent_pool_) {
        renderer_->render_system("Dialogue stub: " + input + " (AI not initialized)");
        return;
    }

    auto npc_it = world_.npcs.find(npc_id);
    if (npc_it == world_.npcs.end()) {
        renderer_->render_error("NPC not found.");
        return;
    }
    
    const auto& npc = npc_it->second;
    
    try {
        auto handle = agent_pool_->acquire(npc_id);
        auto* adapter = dynamic_cast<ZooAgentAdapter*>(&*handle);
        if (!adapter) {
            renderer_->render_error("Failed to cast to ZooAgentAdapter.");
            return;
        }
        
        tool_registry_->register_tools(adapter->agent(), npc_id);
        
        PromptBuilder::Budget budget;
        PromptBuilder pb(budget);
        
        std::string sys_prompt = pb.build_system_prompt(npc.identity, npc.state, world_);
        adapter->set_system_prompt(sys_prompt);
        
        std::string user_msg = pb.build_user_turn(input, world_.player);
        
        renderer_->begin_npc_dialogue(npc.identity.name);
        
        auto chat_handle = adapter->agent().chat(user_msg, {}, [this](std::string_view t) {
            response_handler_->on_token(t);
        });
        
        auto result = chat_handle.await_result();
        if (result) {
            renderer_->flush_dialogue();
            process_pending_mutations();
        } else {
            renderer_->render_error("Agent chat failed.");
        }
        
    } catch (const std::exception& e) {
        renderer_->render_error(std::string("Dialogue error: ") + e.what());
    }
}

} // namespace chronicle
