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
#include "diagnostics/logger.hpp"
#include "engine/mutation_gate.hpp"
#include "engine/mutations.hpp"
#include "engine/parse_utils.hpp"
#include "engine/text_utils.hpp"
#include "entities/world_loader.hpp"
#include "rendering/terminal_renderer.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace chronicle {

namespace {

std::string_view command_verb_name(CommandVerb verb) {
    switch (verb) {
    case CommandVerb::Go:
        return "go";
    case CommandVerb::Look:
        return "look";
    case CommandVerb::Examine:
        return "examine";
    case CommandVerb::Take:
        return "take";
    case CommandVerb::Drop:
        return "drop";
    case CommandVerb::Use:
        return "use";
    case CommandVerb::Give:
        return "give";
    case CommandVerb::Talk:
        return "talk";
    case CommandVerb::Inventory:
        return "inventory";
    case CommandVerb::Save:
        return "save";
    case CommandVerb::Load:
        return "load";
    case CommandVerb::Quit:
        return "quit";
    case CommandVerb::Help:
        return "help";
    case CommandVerb::Dialogue:
        return "dialogue";
    case CommandVerb::Unknown:
        return "unknown";
    }
    return "unknown";
}

std::string_view mutation_type_name(MutationRequest::Type type) {
    switch (type) {
    case MutationRequest::Type::GiveItemToPlayer:
        return "give_item_to_player";
    case MutationRequest::Type::TakeItemFromPlayer:
        return "take_item_from_player";
    case MutationRequest::Type::UpdateNpcMood:
        return "update_npc_mood";
    case MutationRequest::Type::UpdateNpcTrust:
        return "update_npc_trust";
    case MutationRequest::Type::MoveNpc:
        return "move_npc";
    case MutationRequest::Type::RevealKnowledge:
        return "reveal_knowledge";
    case MutationRequest::Type::AddMemory:
        return "add_memory";
    case MutationRequest::Type::SetFlag:
        return "set_flag";
    case MutationRequest::Type::PlayerMove:
        return "player_move";
    case MutationRequest::Type::PlayerTake:
        return "player_take";
    case MutationRequest::Type::PlayerDrop:
        return "player_drop";
    case MutationRequest::Type::SpawnItem:
        return "spawn_item";
    }
    return "unknown";
}

std::string format_params(const std::map<std::string, std::string> &params) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto &[key, value] : params) {
        if (!first) {
            oss << ", ";
        }
        first = false;
        oss << key << "=" << value;
    }
    oss << "}";
    return oss.str();
}

std::optional<std::string> event_param(const EventAction &action, const std::string &key) {
    auto it = action.params.find(key);
    if (it == action.params.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second;
}

bool event_condition_matches(const World &world, const Condition &condition) {
    switch (condition.type) {
    case ConditionType::ClockIs:
        if (condition.args.size() != 1) {
            return false;
        }
        try {
            return world.clock.period == string_to_time_period(condition.args[0]);
        } catch (const std::exception &) {
            return false;
        }
    case ConditionType::PlayerAt:
        return condition.args.size() == 1 && world.player.current_location == condition.args[0];
    case ConditionType::FlagSet: {
        if (condition.args.size() != 2) {
            return false;
        }
        auto flag_it = world.flags.find(condition.args[0]);
        auto expected = parse_bool(condition.args[1]);
        return flag_it != world.flags.end() && expected && flag_it->second == *expected;
    }
    case ConditionType::NpcTrustGe: {
        if (condition.args.size() != 2) {
            return false;
        }
        auto npc_it = world.npcs.find(condition.args[0]);
        auto threshold = parse_int(condition.args[1]);
        return npc_it != world.npcs.end() && threshold &&
               npc_it->second.state.trust_toward_player >= *threshold;
    }
    case ConditionType::NpcAt: {
        if (condition.args.size() != 2) {
            return false;
        }
        auto npc_it = world.npcs.find(condition.args[0]);
        return npc_it != world.npcs.end() &&
               npc_it->second.state.current_location == condition.args[1];
    }
    case ConditionType::ItemInPlayerInv:
        return condition.args.size() == 1 &&
               std::ranges::contains(world.player.inventory, condition.args[0]);
    case ConditionType::TurnGe: {
        if (condition.args.size() != 1) {
            return false;
        }
        auto threshold = parse_int(condition.args[0]);
        return threshold && world.total_turns_elapsed >= *threshold;
    }
    }
    return false;
}

bool event_conditions_match(const World &world, const EventTrigger &event) {
    return std::ranges::all_of(event.conditions, [&](const Condition &condition) {
        return event_condition_matches(world, condition);
    });
}

} // namespace

GameEngine::GameEngine(const std::string &config_path, const WorldFileSet &world_files,
                       std::unique_ptr<Renderer> renderer, std::unique_ptr<NpcAgentPool> agent_pool)
    : config_(Config::load(config_path)), world_(load_world(world_files)), parser_(config_path),
      renderer_(std::move(renderer)),
      save_system_(config_.save_directory.empty() ? "saves" : config_.save_directory),
      agent_pool_(std::move(agent_pool)) {

    logging::write(logging::Level::Info, "engine",
                   "loaded config=" + config_path + " world=" + world_files.world.string() +
                       " locations=" + std::to_string(world_.locations.size()) +
                       " npcs=" + std::to_string(world_.npcs.size()) +
                       " items=" + std::to_string(world_.items.size()));

    if (!renderer_) {
        renderer_ = std::make_unique<TerminalRenderer>();
    }

    tool_registry_ = std::make_unique<ToolRegistry>(
        world_, [this](MutationRequest req) { pending_mutations_.push_back(std::move(req)); });
    response_handler_ = std::make_unique<ResponseHandler>(
        [this](std::string_view narration) { renderer_->render_action(narration); }, token_queue_,
        world_, config_.mutation_narration_templates);

    if (!agent_pool_ && !config_.model_path.empty()) {
        try {
            logging::write(logging::Level::Info, "ai",
                           "initializing agent pool model_path=" + config_.model_path +
                               " context_size=" + std::to_string(config_.context_size) +
                               " n_gpu_layers=" + std::to_string(config_.n_gpu_layers));
            agent_pool_ = std::make_unique<NpcAgentPool>(NpcAgentPool::from_config(config_));
        } catch (const std::exception &e) {
            logging::write(logging::Level::Error, "ai",
                           std::string("failed to initialize agent pool: ") + e.what());
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
    logging::write(logging::Level::Debug, "engine",
                   "handle_command verb=" + std::string(command_verb_name(cmd.verb)) +
                       " raw_input=\"" + cmd.raw_input + "\" primary_arg=\"" + cmd.primary_arg +
                       "\"");

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
        auto result = validate_player_move(world_, cmd.primary_arg);
        if (auto *error = std::get_if<std::string>(&result)) {
            renderer_->render_error(*error);
        } else {
            auto &req = std::get<MutationRequest>(result);
            auto dir = req.params.at("direction");
            auto dest_id = req.params.at("location_id");
            pending_mutations_.push_back(std::move(req));
            run_post_turn_pipeline();
            auto dest_it = world_.locations.find(dest_id);
            std::string name = dest_it != world_.locations.end() ? dest_it->second.name : dest_id;
            renderer_->render_move(dir, name);
            render_current_scene();
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
        auto result = validate_player_take(world_, cmd.primary_arg);
        if (auto *error = std::get_if<std::string>(&result)) {
            renderer_->render_error(*error);
        } else {
            auto &req = std::get<MutationRequest>(result);
            auto item_id = req.params.at("item_id");
            pending_mutations_.push_back(std::move(req));
            run_post_turn_pipeline();
            auto w_it = world_.items.find(item_id);
            renderer_->render_action(
                "You take the " + (w_it != world_.items.end() ? w_it->second.name : item_id) + ".");
        }
        return;
    }

    if (cmd.verb == CommandVerb::Drop) {
        auto result = validate_player_drop(world_, cmd.primary_arg);
        if (auto *error = std::get_if<std::string>(&result)) {
            renderer_->render_error(*error);
        } else {
            auto &req = std::get<MutationRequest>(result);
            auto item_id = req.params.at("item_id");
            pending_mutations_.push_back(std::move(req));
            run_post_turn_pipeline();
            auto w_it = world_.items.find(item_id);
            renderer_->render_action(
                "You drop the " + (w_it != world_.items.end() ? w_it->second.name : item_id) + ".");
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
        active_conversation_handle_.reset();
        current_conversation_npc_id_.reset();
        phase_ = GamePhase::Playing;
        pending_mutations_.clear();
        tool_registry_->clear_all();
        token_queue_.reset();
        renderer_->render_system("Loaded game from slot " + std::to_string(*slot) + ".");
        render_current_scene();
        return;
    }

    if (cmd.verb == CommandVerb::Talk) {
        if (auto npc_id = find_visible_npc_id(cmd.primary_arg)) {
            start_conversation(*npc_id);
        } else {
            renderer_->render_error("There is no one here by that name.");
        }
        return;
    }

    renderer_->render_error("You can't do that right now.");
}

void GameEngine::process_pending_mutations() {
    if (pending_mutations_.empty()) {
        return;
    }

    logging::write(logging::Level::Debug, "mutations",
                   "processing pending count=" + std::to_string(pending_mutations_.size()));

    std::vector<MutationRequest> applied;
    applied.reserve(pending_mutations_.size());
    for (const auto &m : pending_mutations_) {
        if (apply_mutation(world_, m)) {
            logging::write(logging::Level::Info, "mutations",
                           "applied type=" + std::string(mutation_type_name(m.type)) +
                               " actor=" + m.actor_id + " params=" + format_params(m.params));
            applied.push_back(m);
        } else {
            logging::write(logging::Level::Warning, "mutations",
                           "rejected type=" + std::string(mutation_type_name(m.type)) +
                               " actor=" + m.actor_id + " params=" + format_params(m.params));
        }
    }
    pending_mutations_.clear();

    for (const auto &mutation : applied) {
        std::string actor_name = mutation.actor_id;
        if (!mutation.actor_id.empty()) {
            if (auto it = world_.npcs.find(mutation.actor_id); it != world_.npcs.end()) {
                actor_name = it->second.identity.name;
            }
        }
        response_handler_->narrate_mutation(mutation, actor_name);
    }

    if (current_conversation_npc_id_ && !player_can_see_npc(*current_conversation_npc_id_)) {
        active_conversation_handle_.reset();
        current_conversation_npc_id_.reset();
        phase_ = GamePhase::Playing;
    }
}

void GameEngine::run_post_turn_pipeline() {
    process_pending_mutations();

    const auto previous_period = world_.clock.period;
    const auto previous_day = world_.clock.day;
    world_.clock.advance_turn(config_.turns_per_period);
    world_.total_turns_elapsed = world_.clock.total_turns;

    if (world_.clock.period != previous_period || world_.clock.day != previous_day) {
        renderer_->render_time_advance(world_.clock);
    }

    evaluate_scripted_events();
    process_pending_mutations();
}

void GameEngine::evaluate_scripted_events() {
    for (auto &event : world_.events) {
        if (event.once && event.fired) {
            continue;
        }
        if (!event_conditions_match(world_, event)) {
            continue;
        }

        logging::write(logging::Level::Info, "events", "triggering event=" + event.id);

        bool entered_resolution = false;
        for (const auto &action : event.actions) {
            if (action.type == "move_npc") {
                auto npc_id = event_param(action, "npc_id");
                auto location_id = event_param(action, "location_id");
                if (!npc_id || !location_id) {
                    logging::write(logging::Level::Warning, "events",
                                   "skipping malformed move_npc action event=" + event.id);
                    continue;
                }
                pending_mutations_.push_back(MutationRequest{
                    .type = MutationRequest::Type::MoveNpc,
                    .source = MutationRequest::Source::System,
                    .actor_id = *npc_id,
                    .params = {{"location_id", *location_id}},
                });
            } else if (action.type == "set_flag") {
                auto flag_id = event_param(action, "flag_id");
                auto value = event_param(action, "value");
                if (!flag_id || !value) {
                    logging::write(logging::Level::Warning, "events",
                                   "skipping malformed set_flag action event=" + event.id);
                    continue;
                }
                pending_mutations_.push_back(MutationRequest{
                    .type = MutationRequest::Type::SetFlag,
                    .source = MutationRequest::Source::System,
                    .actor_id = event.id,
                    .params = {{"flag_id", *flag_id}, {"value", *value}},
                });
            } else if (action.type == "spawn_item") {
                auto item_id = event_param(action, "item_id");
                auto location_id = event_param(action, "location_id");
                if (!item_id || !location_id) {
                    logging::write(logging::Level::Warning, "events",
                                   "skipping malformed spawn_item action event=" + event.id);
                    continue;
                }
                pending_mutations_.push_back(MutationRequest{
                    .type = MutationRequest::Type::SpawnItem,
                    .source = MutationRequest::Source::System,
                    .actor_id = event.id,
                    .params = {{"item_id", *item_id}, {"location_id", *location_id}},
                });
            } else if (action.type == "narrate") {
                if (auto text = event_param(action, "text")) {
                    renderer_->render_action(*text);
                }
            } else if (action.type == "end_game") {
                phase_ = GamePhase::Resolution;
                running_ = false;
                renderer_->render_resolution("");
                entered_resolution = true;
                break;
            } else {
                logging::write(logging::Level::Warning, "events",
                               "skipping unknown action type=" + action.type +
                                   " event=" + event.id);
            }
        }

        if (event.once) {
            event.fired = true;
        }
        if (entered_resolution) {
            break;
        }
    }
}

void GameEngine::handle_dialogue(const std::string &npc_id, const std::string &input) {
    if (!active_conversation_handle_) {
        if (!agent_pool_) {
            logging::write(logging::Level::Warning, "dialogue",
                           "agent pool unavailable; using stub dialogue for npc=" + npc_id);
            renderer_->render_system("Dialogue stub: " + input + " (AI not initialized)");
            run_post_turn_pipeline();
            return;
        }
        logging::write(logging::Level::Error, "dialogue",
                       "no active conversation handle for npc=" + npc_id);
        renderer_->render_error("No active conversation.");
        return;
    }

    auto npc_it = world_.npcs.find(npc_id);
    if (npc_it == world_.npcs.end()) {
        logging::write(logging::Level::Error, "dialogue", "npc not found id=" + npc_id);
        renderer_->render_error("NPC not found.");
        return;
    }

    const auto &npc = npc_it->second;

    try {
        pending_mutations_.clear();
        tool_registry_->clear_all();
        token_queue_.reset();

        logging::write(logging::Level::Info, "dialogue",
                       "starting npc=" + npc_id + " player_input=\"" + input + "\"");

        PromptBuilder::Budget budget{
            .max_memory_tokens = config_.max_memory_tokens,
            .max_world_tokens = config_.max_world_tokens,
            .max_history_tokens = config_.max_history_tokens,
        };
        PromptBuilder pb(budget);

        std::string dynamic_ctx = pb.build_dynamic_context(npc.identity, npc.state, world_);
        (*active_conversation_handle_)->add_system_message(dynamic_ctx);

        std::string user_msg = pb.build_user_turn(input, world_.player);

        logging::write(logging::Level::Debug, "dialogue",
                       "user_turn npc=" + npc_id + " chars=" + std::to_string(user_msg.size()) +
                           "\n" + user_msg);

        renderer_->begin_npc_dialogue(npc.identity.name);

        bool streamed_any = false;
        auto poll = [this, &streamed_any]() { streamed_any = drain_token_queue() || streamed_any; };
        auto result =
            (*active_conversation_handle_)
                ->chat_streaming(
                    user_msg, [this](std::string_view t) { response_handler_->on_token(t); }, poll);

        poll();

        logging::write(logging::Level::Info, "dialogue",
                       "chat completed npc=" + npc_id +
                           " success=" + (result.success ? std::string("true") : "false") +
                           " streamed_any=" + (streamed_any ? std::string("true") : "false"));

        if (!streamed_any) {
            for (const auto &[dialogue_npc_id, dialogue] : tool_registry_->drain_dialogue_log()) {
                if (dialogue_npc_id == npc_id) {
                    renderer_->stream_token(dialogue);
                    streamed_any = true;
                }
            }
        }
        renderer_->flush_dialogue();

        if (result.success) {
            run_post_turn_pipeline();
        } else {
            pending_mutations_.clear();
            tool_registry_->clear_all();
            logging::write(logging::Level::Error, "dialogue",
                           "agent chat failed npc=" + npc_id + " error=" + result.error_message);
            renderer_->render_error("Agent chat failed: " + result.error_message);
        }

    } catch (const std::exception &e) {
        pending_mutations_.clear();
        tool_registry_->clear_all();
        logging::write(logging::Level::Error, "dialogue",
                       "dialogue exception npc=" + npc_id + " error=" + e.what());
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

bool GameEngine::start_conversation(const std::string &npc_id) {
    auto npc_it = world_.npcs.find(npc_id);
    if (npc_it == world_.npcs.end()) {
        renderer_->render_error("NPC not found.");
        return false;
    }

    if (!agent_pool_) {
        logging::write(logging::Level::Warning, "dialogue",
                       "agent pool unavailable; conversation with " + npc_id +
                           " will use stub dialogue");
        phase_ = GamePhase::InConversation;
        current_conversation_npc_id_ = npc_id;
        renderer_->render_system("You are now talking to " + npc_it->second.identity.name + ".");
        return true;
    }

    try {
        auto handle = agent_pool_->acquire(npc_id);
        handle->register_tools(*tool_registry_, npc_id);

        PromptBuilder::Budget budget{
            .max_memory_tokens = config_.max_memory_tokens,
            .max_world_tokens = config_.max_world_tokens,
            .max_history_tokens = config_.max_history_tokens,
        };
        PromptBuilder pb(budget);

        const auto &npc = npc_it->second;
        std::string sys_prompt = pb.build_static_system_prompt(npc.identity, world_);
        handle->set_system_prompt(sys_prompt);

        logging::write(logging::Level::Debug, "dialogue",
                       "static_system_prompt npc=" + npc_id +
                           " chars=" + std::to_string(sys_prompt.size()) + "\n" + sys_prompt);

        active_conversation_handle_ = std::move(handle);
        phase_ = GamePhase::InConversation;
        current_conversation_npc_id_ = npc_id;
        renderer_->render_system("You are now talking to " + npc.identity.name + ".");

        logging::write(logging::Level::Info, "dialogue", "conversation started npc=" + npc_id);
        return true;

    } catch (const std::exception &e) {
        logging::write(logging::Level::Error, "dialogue",
                       "start_conversation failed npc=" + npc_id + " error=" + e.what());
        renderer_->render_error(std::string("Could not start conversation: ") + e.what());
        return false;
    }
}

void GameEngine::leave_conversation() {
    active_conversation_handle_.reset();
    current_conversation_npc_id_.reset();
    phase_ = GamePhase::Playing;
    renderer_->render_system("You step out of the conversation.");
}

bool GameEngine::is_conversation_exit(std::string_view input) const {
    auto normalized = text::trim_and_lower(input);
    return normalized == "bye" || normalized == "goodbye" || normalized == "leave" ||
           normalized == "exit conversation" || normalized == "leave conversation";
}

} // namespace chronicle
