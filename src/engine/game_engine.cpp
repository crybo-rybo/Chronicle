/**
 * @file game_engine.cpp
 * @brief Implementation of @ref GameEngine — the main game loop and command dispatcher.
 *
 * @details Implements construction (subsystem wiring), the blocking @ref GameEngine::run
 * loop, command dispatch, the dialogue pipeline (prompt build → agent chat →
 * token streaming → mutation application), and all private helper utilities.
 */

#include "engine/game_engine.hpp"
#include "ai/prompt_builder.hpp"
#include "engine/mutations.hpp"
#include "entities/world_loader.hpp"
#include "engine/text_utils.hpp"
#include "rendering/terminal_renderer.hpp"
#include <algorithm>
#include <iostream>

namespace chronicle {

GameEngine::GameEngine(const std::string &config_path, const std::string &data_dir,
                       std::unique_ptr<Renderer> renderer,
                       std::unique_ptr<NpcAgentPool> agent_pool)
    : config_(Config::load(config_path)),
      world_(load_world(data_dir)),
      parser_(config_path),
      renderer_(std::move(renderer)),
      save_system_(config_.save_directory.empty() ? "saves" : config_.save_directory),
      agent_pool_(std::move(agent_pool)) {

    if (!renderer_) {
        renderer_ = std::make_unique<TerminalRenderer>();
    }

    tool_registry_ = std::make_unique<ToolRegistry>(world_);
    response_handler_ = std::make_unique<ResponseHandler>(
        *renderer_, token_queue_, world_, config_.mutation_narration_templates);
    
    if (!agent_pool_ && !config_.model_path.empty()) {
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
        drain_token_queue();

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

bool GameEngine::drain_token_queue() {
    bool rendered = false;
    while (auto token = token_queue_.try_pop()) {
        renderer_->stream_token(*token);
        rendered = true;
    }
    return rendered;
}

void GameEngine::handle_command(const ParsedCommand &cmd) {
    if (cmd.verb == CommandVerb::Unknown) {
        renderer_->render_error("I don't understand that command.");
        return;
    }
    
    if (cmd.verb == CommandVerb::Dialogue) {
        if (is_conversation_exit(cmd.raw_input)) {
            leave_conversation();
            return;
        }
        if (!current_conversation_npc_id_) {
            renderer_->render_error("You are not talking to anyone.");
            phase_ = GamePhase::Playing;
            return;
        }
        handle_dialogue(*current_conversation_npc_id_, cmd.raw_input);
        return;
    }

    if (phase_ == GamePhase::InConversation && cmd.verb == CommandVerb::Quit &&
        is_conversation_exit(cmd.raw_input)) {
        leave_conversation();
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
                    return !w_it->second.hidden && w_it->second.takeable &&
                           (text::contains_normalized(w_it->second.name, item_name) ||
                            text::contains_normalized(id, item_name));
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
                return text::contains_normalized(w_it->second.name, item_name) ||
                       text::contains_normalized(id, item_name);
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

    if (cmd.verb == CommandVerb::Examine) {
        if (auto item_id = find_accessible_item_id(cmd.primary_arg)) {
            renderer_->render_item_examine(world_.items.at(*item_id));
        } else {
            renderer_->render_error("You don't see that here.");
        }
        return;
    }

    if (cmd.verb == CommandVerb::Save) {
        auto slot = parse_slot(cmd.primary_arg);
        if (!slot) {
            renderer_->render_error("Save slot must be a positive number.");
            return;
        }
        try {
            save_system_.save(world_, *slot);
            renderer_->render_system("Saved game to slot " + std::to_string(*slot) + ".");
        } catch (const std::exception &e) {
            renderer_->render_error(std::string("Save failed: ") + e.what());
        }
        return;
    }

    if (cmd.verb == CommandVerb::Load) {
        auto slot = parse_slot(cmd.primary_arg);
        if (!slot) {
            renderer_->render_error("Load slot must be a positive number.");
            return;
        }
        auto save_data = save_system_.load(*slot);
        if (!save_data) {
            renderer_->render_error("No valid save found in slot " + std::to_string(*slot) + ".");
            return;
        }
        world_ = std::move(save_data->world);
        current_conversation_npc_id_.reset();
        phase_ = GamePhase::Playing;
        tool_registry_->clear_all();
        token_queue_.reset();
        renderer_->render_system("Loaded game from slot " + std::to_string(*slot) + ".");
        render_current_scene();
        return;
    }
    
    if (cmd.verb == CommandVerb::Talk) {
        // Find NPC locally
        if (auto npc_id = find_visible_npc_id(cmd.primary_arg)) {
            phase_ = GamePhase::InConversation;
            current_conversation_npc_id_ = *npc_id;
            renderer_->render_system("You are now talking to " +
                                     world_.npcs.at(*npc_id).identity.name + ".");
        } else {
            renderer_->render_error("There is no one here by that name.");
        }
        return;
    }

    renderer_->render_error("You can't do that right now.");
}

void GameEngine::process_pending_mutations() {
    auto mutations = tool_registry_->pending_mutations();
    if (!mutations.empty()) {
        std::vector<MutationRequest> applied;
        applied.reserve(mutations.size());
        for (const auto &m : mutations) {
            if (apply_mutation(world_, m)) {
                applied.push_back(m);
            }
        }
        for (const auto &mutation : applied) {
            std::string npc_name = mutation.npc_id;
            if (!mutation.npc_id.empty() && world_.npcs.contains(mutation.npc_id)) {
                npc_name = world_.npcs.at(mutation.npc_id).identity.name;
            }
            response_handler_->narrate_mutations({mutation}, npc_name);
        }
        tool_registry_->clear_pending();

        if (current_conversation_npc_id_ && !player_can_see_npc(*current_conversation_npc_id_)) {
            current_conversation_npc_id_.reset();
            phase_ = GamePhase::Playing;
        }
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
        tool_registry_->clear_all();
        token_queue_.reset();

        auto handle = agent_pool_->acquire(npc_id);
        handle->register_tools(*tool_registry_, npc_id);
        
        PromptBuilder::Budget budget;
        PromptBuilder pb(budget);
        
        std::string sys_prompt = pb.build_system_prompt(npc.identity, npc.state, world_);
        handle->set_system_prompt(sys_prompt);
        
        std::string user_msg = pb.build_user_turn(input, world_.player);
        
        renderer_->begin_npc_dialogue(npc.identity.name);
        
        bool streamed_any = false;
        auto poll = [this, &streamed_any]() { streamed_any = drain_token_queue() || streamed_any; };
        auto result = handle->chat_streaming(
            user_msg,
            [this](std::string_view t) { response_handler_->on_token(t); },
            poll);

        poll();

        if (!streamed_any) {
            for (const auto &[dialogue_npc_id, dialogue] : tool_registry_->drain_dialogue_log()) {
                if (dialogue_npc_id == npc_id) {
                    renderer_->stream_token(dialogue);
                    streamed_any = true;
                }
            }
        }
        renderer_->flush_dialogue();

        // Always process mutations — tool lambdas validate and enqueue during inference,
        // so mutations are valid even if the chat was cut short (e.g., iteration limit).
        process_pending_mutations();

        if (!result.success) {
            renderer_->render_error("Agent chat failed: " + result.error_message);
        }
        
    } catch (const std::exception& e) {
        renderer_->render_error(std::string("Dialogue error: ") + e.what());
    }
}

bool GameEngine::player_can_see_item(const std::string &item_id) const {
    auto loc_it = world_.locations.find(world_.player.current_location);
    if (loc_it == world_.locations.end()) {
        return false;
    }
    return std::ranges::contains(loc_it->second.items, item_id);
}

bool GameEngine::player_can_see_npc(const std::string &npc_id) const {
    auto loc_it = world_.locations.find(world_.player.current_location);
    if (loc_it == world_.locations.end()) {
        return false;
    }
    return std::ranges::contains(loc_it->second.npcs, npc_id);
}

std::optional<std::string> GameEngine::find_visible_npc_id(const std::string &query) const {
    auto loc_it = world_.locations.find(world_.player.current_location);
    if (loc_it == world_.locations.end()) {
        return std::nullopt;
    }

    for (const auto &npc_id : loc_it->second.npcs) {
        auto npc_it = world_.npcs.find(npc_id);
        if (npc_it != world_.npcs.end() &&
            (text::contains_normalized(npc_it->second.identity.name, query) ||
             text::contains_normalized(npc_id, query))) {
            return npc_id;
        }
    }
    return std::nullopt;
}

std::optional<std::string> GameEngine::find_accessible_item_id(const std::string &query) const {
    for (const auto &item_id : world_.player.inventory) {
        auto item_it = world_.items.find(item_id);
        if (item_it != world_.items.end() &&
            (text::contains_normalized(item_it->second.name, query) ||
             text::contains_normalized(item_id, query))) {
            return item_id;
        }
    }

    auto loc_it = world_.locations.find(world_.player.current_location);
    if (loc_it == world_.locations.end()) {
        return std::nullopt;
    }

    for (const auto &item_id : loc_it->second.items) {
        auto item_it = world_.items.find(item_id);
        if (item_it != world_.items.end() && !item_it->second.hidden &&
            (text::contains_normalized(item_it->second.name, query) ||
             text::contains_normalized(item_id, query))) {
            return item_id;
        }
    }
    return std::nullopt;
}

std::optional<int> GameEngine::parse_slot(const std::string &slot_text) const {
    auto trimmed = text::trim_copy(slot_text);
    if (trimmed.empty()) {
        return 1;
    }
    try {
        std::size_t parsed = 0;
        int slot = std::stoi(trimmed, &parsed);
        if (parsed != trimmed.size() || slot <= 0) {
            return std::nullopt;
        }
        return slot;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

void GameEngine::leave_conversation() {
    current_conversation_npc_id_.reset();
    phase_ = GamePhase::Playing;
    renderer_->render_system("You step out of the conversation.");
}

bool GameEngine::is_conversation_exit(std::string_view input) const {
    auto normalized = text::to_lower_copy(text::trim_copy(input));
    return normalized == "bye" || normalized == "goodbye" || normalized == "leave" ||
           normalized == "exit conversation" || normalized == "leave conversation";
}

} // namespace chronicle
