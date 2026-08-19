#include "chronicle/game/cartridge_game.hpp"

#include <regex>
#include <sstream>

#include "chronicle/cartridge/loader.hpp"
#include "chronicle/cartridge/validator.hpp"
#include "chronicle/game/text_utils.hpp"

namespace chronicle {

namespace {

const std::set<std::string> EXIT_PHRASES = {"bye", "goodbye", "leave", "exit conversation"};
const std::set<std::string> HARD_CONVERSATION_COMMANDS = {"look", "inventory", "help",
                                                          "quit", "save",      "load"};
const std::set<std::string> BLOCKED_CONVERSATION_COMMANDS = {"go",   "examine", "take",
                                                             "drop", "use",     "talk"};

using game_detail::lower;
using game_detail::parse_int;
using game_detail::split_command;
using game_detail::split_words;
using game_detail::strip;

std::filesystem::path
save_directory_for(const WorldState &world,
                   const std::optional<std::filesystem::path> &override_directory) {
    if (!is_safe_cartridge_id(world.manifest.id)) {
        throw std::invalid_argument("Unsafe cartridge id cannot be used for saves: " +
                                    world.manifest.id);
    }
    return override_directory.value_or(std::filesystem::current_path() / "saves" /
                                       world.manifest.id);
}

} // namespace

CartridgeGame::CartridgeGame(const std::filesystem::path &package_dir,
                             std::optional<std::filesystem::path> save_dir)
    : CartridgeGame(load_package(package_dir), std::move(save_dir)) {}

CartridgeGame::CartridgeGame(WorldState world, std::optional<std::filesystem::path> save_dir)
    : world_(std::move(world)), action_gate_(world_, phase_, active_npc_, significant_),
      saves_(save_directory_for(world_, save_dir), world_) {}

void CartridgeGame::set_conversation_hooks(SnapshotHook snapshot, RestoreHook restore) {
    conversation_snapshot_ = std::move(snapshot);
    conversation_restore_ = std::move(restore);
}

GameEvents CartridgeGame::bootstrap() {
    GameEvents events{{EventKind::title, "— " + world_.manifest.name + " —"}};
    auto looked = look();
    events.insert(events.end(), looked.begin(), looked.end());
    events.push_back({EventKind::hint, "Type 'help' for commands."});
    return events;
}

PlayerDispatch CartridgeGame::dispatch_player(const std::string &text) {
    const std::string raw = strip(text);
    if (raw.empty()) {
        return {};
    }
    if (phase_ == GamePhase::game_over) {
        return {.events = {{EventKind::narration, "The scenario has ended. Type quit to exit."}},
                .npc_turn = std::nullopt};
    }

    const std::string expanded = expand_alias(raw);
    const std::string lowered = strip(lower(expanded));

    if (phase_ == GamePhase::in_conversation) {
        if (EXIT_PHRASES.contains(lowered)) {
            (void)submit_world_action(actions::EndConversation{});
            return {.events = {{EventKind::narration, "You end the conversation."}},
                    .npc_turn = std::nullopt};
        }
        const auto [first, ignored] = split_command(lowered);
        if (HARD_CONVERSATION_COMMANDS.contains(first)) {
            return {.events = dispatch_playing(expanded), .npc_turn = std::nullopt};
        }
        if (BLOCKED_CONVERSATION_COMMANDS.contains(first)) {
            return {
                .events = {{EventKind::narration, "Finish the conversation first (say 'bye')."}},
                .npc_turn = std::nullopt};
        }
        return {.events = {},
                .npc_turn = NpcTurnRequest{.npc_id = *active_npc_, .player_text = raw}};
    }

    return {.events = dispatch_playing(expanded), .npc_turn = std::nullopt};
}

actions::ActionOutcome CartridgeGame::submit_world_action(actions::WorldAction action) {
    return action_gate_.submit(std::move(action));
}

RuntimeCheckpoint CartridgeGame::checkpoint_runtime() const {
    return RuntimeCheckpoint{
        .world = world_,
        .phase = phase_,
        .active_npc = active_npc_,
        .significant = significant_,
    };
}

actions::ActionOutcome CartridgeGame::restore_runtime(RuntimeCheckpoint checkpoint) {
    return submit_world_action(actions::RestoreRuntime{
        .world = std::move(checkpoint.world),
        .phase = checkpoint.phase,
        .active_npc = std::move(checkpoint.active_npc),
        .significant = checkpoint.significant,
    });
}

GameEvents CartridgeGame::after_turn() {
    GameEvents events;
    if (significant_ && phase_ != GamePhase::game_over) {
        (void)submit_world_action(actions::AdvanceClock{});
    }
    auto fired = evaluate_events();
    events.insert(events.end(), fired.begin(), fired.end());
    if (phase_ != GamePhase::game_over && world_.clock.time_expired()) {
        (void)submit_world_action(actions::EndGame{});
        events.push_back({EventKind::ending,
                          "Time has run out. The scenario ends without a clearer resolution."});
    }
    return events;
}

SaveResult CartridgeGame::save(const int slot) {
    try {
        nlohmann::json conversations = nlohmann::json::object();
        if (conversation_snapshot_) {
            auto snapshot = conversation_snapshot_();
            if (!snapshot) {
                return std::unexpected(
                    SaveError{.kind = SaveError::Kind::io,
                              .message = "Could not snapshot conversations: " + snapshot.error()});
            }
            conversations = std::move(*snapshot);
        }
        return saves_.save(slot, world_, phase_, active_npc_, conversations);
    } catch (const std::exception &exception) {
        return std::unexpected(SaveError{.kind = SaveError::Kind::io,
                                         .message = "Could not snapshot conversations: " +
                                                    std::string(exception.what())});
    }
}

std::string CartridgeGame::help_text() const {
    if (phase_ == GamePhase::in_conversation) {
        return "In conversation: speak freely, or bye/look/inventory/save/load/help/quit.";
    }
    return "Commands: go <dir>, look, examine <item>, take/drop <item>, "
           "use <item> on <target>, talk <npc>, inventory, save [n], load [n], help, quit";
}

// --- player commands -----------------------------------------------------

std::string CartridgeGame::expand_alias(const std::string &text) const {
    const auto [first, rest] = split_command(text);
    const auto it = world_.config.verb_aliases.find(first);
    if (it == world_.config.verb_aliases.end()) {
        return text;
    }
    return rest.empty() ? it->second : it->second + " " + rest;
}

GameEvents CartridgeGame::dispatch_playing(const std::string &text) {
    const auto [cmd, arg] = split_command(text);
    if (cmd.empty()) {
        return {};
    }
    const auto words = split_words(lower(text));

    if (cmd == "quit" || cmd == "q") {
        (void)submit_world_action(actions::EndGame{});
        return {{EventKind::system, "Goodbye."}};
    }
    if (cmd == "help") {
        return {{EventKind::narration, help_text()}};
    }
    if (cmd == "look") {
        return look();
    }
    if (cmd == "inventory") {
        return show_inventory();
    }
    if (cmd == "go") {
        return go(words.size() > 1 ? words[1] : "");
    }
    if (cmd == "examine") {
        return examine(arg);
    }
    if (cmd == "take") {
        return take(arg);
    }
    if (cmd == "drop") {
        return drop(arg);
    }
    if (cmd == "use") {
        return use(arg);
    }
    if (cmd == "talk") {
        return talk(arg);
    }
    if (cmd == "save") {
        if (words.size() > 2) {
            return {{EventKind::warning, "Usage: save [slot 1-99]"}};
        }
        const auto parsed_slot = words.size() == 2 ? parse_int(words[1]) : std::optional<int>{1};
        if (!parsed_slot) {
            return {{EventKind::warning, "Save slot must be an integer from 1 to 99."}};
        }
        const int slot = *parsed_slot;
        if (auto saved = save(slot); !saved) {
            return {{EventKind::warning,
                     "Could not save slot " + std::to_string(slot) + ": " + saved.error().message}};
        }
        return {{EventKind::narration, "Saved to slot " + std::to_string(slot) + "."}};
    }
    if (cmd == "load") {
        if (words.size() > 2) {
            return {{EventKind::warning, "Usage: load [slot 1-99]"}};
        }
        const auto parsed_slot = words.size() == 2 ? parse_int(words[1]) : std::optional<int>{1};
        if (!parsed_slot) {
            return {{EventKind::warning, "Load slot must be an integer from 1 to 99."}};
        }
        return do_load(*parsed_slot);
    }
    return {{EventKind::narration, "Unknown command: " + text}};
}

GameEvents CartridgeGame::do_load(const int slot) {
    auto loaded = saves_.load(slot);
    if (!loaded) {
        if (loaded.error().kind == SaveError::Kind::missing) {
            return {{EventKind::narration, "No save in slot " + std::to_string(slot) + "."}};
        }
        return {{EventKind::warning,
                 "Could not load slot " + std::to_string(slot) + ": " + loaded.error().message}};
    }
    auto checkpoint = checkpoint_runtime();
    auto restored = submit_world_action(actions::RestoreRuntime{
        .world = std::move(loaded->world),
        .phase = loaded->phase,
        .active_npc = std::move(loaded->active_npc),
    });
    if (!restored) {
        return {{EventKind::warning,
                 "Could not load slot " + std::to_string(slot) + ": " + restored.error().reason}};
    }
    if (conversation_restore_) {
        auto conversations = conversation_restore_(loaded->conversations);
        if (!conversations) {
            auto rolled_back = restore_runtime(std::move(checkpoint));
            const std::string rollback =
                rolled_back ? "" : "; rollback failed: " + rolled_back.error().reason;
            return {{EventKind::warning, "Could not load slot " + std::to_string(slot) + ": " +
                                             conversations.error() + rollback}};
        }
    }
    GameEvents events{{EventKind::narration, "Loaded slot " + std::to_string(slot) + "."}};
    auto looked = look();
    events.insert(events.end(), looked.begin(), looked.end());
    return events;
}

GameEvents CartridgeGame::look() const {
    const auto &loc_id = world_.player.current_location;
    const auto &loc = world_.locations.at(loc_id);
    std::vector<std::string> lines = {loc.name, loc.base_description};

    std::vector<std::string> item_names;
    for (const auto &[iid, position] : world_.item_positions) {
        const auto item_it = world_.items.find(iid);
        if (position.is_location(loc_id) && item_it != world_.items.end() &&
            !item_it->second.hidden) {
            item_names.push_back(item_it->second.name);
        }
    }
    if (!item_names.empty()) {
        std::string line = "You see: ";
        for (std::size_t i = 0; i < item_names.size(); ++i) {
            line += (i == 0 ? "" : ", ") + item_names[i];
        }
        lines.push_back(line);
    }

    std::vector<std::string> npc_names;
    for (const auto &[nid, npc] : world_.npcs) {
        if (npc.state.current_location == loc_id) {
            npc_names.push_back(npc.identity.name);
        }
    }
    if (!npc_names.empty()) {
        std::string line = "Here: ";
        for (std::size_t i = 0; i < npc_names.size(); ++i) {
            line += (i == 0 ? "" : ", ") + npc_names[i];
        }
        lines.push_back(line);
    }

    const auto locked = loc.locked_directions();
    std::vector<std::string> exits;
    for (const auto &[direction, dest] : loc.exits) {
        exits.push_back(locked.contains(direction) ? direction + " (locked)" : direction);
    }
    if (!exits.empty()) {
        std::string line = "Exits: ";
        for (std::size_t i = 0; i < exits.size(); ++i) {
            line += (i == 0 ? "" : ", ") + exits[i];
        }
        lines.push_back(line);
    }
    lines.push_back("[" + world_.clock.period_name() + "]");

    std::string text;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        text += (i == 0 ? "" : "\n") + lines[i];
    }
    return {{EventKind::look, text}};
}

GameEvents CartridgeGame::show_inventory() const {
    const auto inventory = items_at(world_, ItemHolder::player);
    if (inventory.empty()) {
        return {{EventKind::narration, "You are carrying nothing."}};
    }
    std::string line = "You are carrying: ";
    bool first = true;
    for (const auto &iid : inventory) {
        const auto it = world_.items.find(iid);
        if (it == world_.items.end()) {
            continue;
        }
        line += (first ? "" : ", ") + it->second.name;
        first = false;
    }
    return {{EventKind::narration, line}};
}

std::optional<std::string> CartridgeGame::resolve_item_ref(const std::string &text) const {
    const std::string needle = strip(lower(text));
    if (needle.empty()) {
        return std::nullopt;
    }
    if (world_.items.contains(needle)) {
        return needle;
    }
    for (const auto &[iid, item] : world_.items) {
        const std::string name = lower(item.name);
        if (name == needle || name.find(needle) != std::string::npos) {
            return iid;
        }
    }
    return std::nullopt;
}

std::optional<std::string> CartridgeGame::resolve_npc_ref(const std::string &text) const {
    const std::string needle = strip(lower(text));
    if (needle.empty()) {
        return std::nullopt;
    }
    if (world_.npcs.contains(needle)) {
        return needle;
    }
    for (const auto &[nid, npc] : world_.npcs) {
        const std::string name = lower(npc.identity.name);
        if (name == needle || name.find(needle) != std::string::npos) {
            return nid;
        }
    }
    return std::nullopt;
}

GameEvents CartridgeGame::go(const std::string &direction_arg) {
    const std::string direction = strip(lower(direction_arg));
    if (direction.empty()) {
        return {{EventKind::narration, "Go where?"}};
    }
    const auto &loc = world_.locations.at(world_.player.current_location);
    const auto exit_it = loc.exits.find(direction);
    if (exit_it == loc.exits.end()) {
        return {{EventKind::narration, "You can't go that way."}};
    }
    if (loc.locked_directions().contains(direction)) {
        return {{EventKind::narration, "That way is locked."}};
    }
    if (auto moved = submit_world_action(actions::MovePlayer{.direction = direction}); !moved) {
        return {{EventKind::warning, moved.error().reason}};
    }
    return look();
}

GameEvents CartridgeGame::examine(const std::string &arg) const {
    const auto item_id = resolve_item_ref(arg);
    if (!item_id || !item_accessible(*item_id)) {
        return {{EventKind::narration, "You don't see that here."}};
    }
    const auto &item = world_.items.at(*item_id);
    std::string text = item.description;
    const auto readable = item.properties.find("readable");
    const auto item_text = item.properties.find("text");
    if (readable != item.properties.end() && readable->second == "true" &&
        item_text != item.properties.end() && !item_text->second.empty()) {
        text += "\nIt reads: " + item_text->second;
    }
    return {{EventKind::narration, text}};
}

bool CartridgeGame::item_accessible(const std::string &item_id) const {
    if (item_is_at(world_, item_id, ItemHolder::player)) {
        return true;
    }
    const auto item = world_.items.find(item_id);
    return item != world_.items.end() && !item->second.hidden &&
           item_is_at(world_, item_id, ItemHolder::location, world_.player.current_location);
}

GameEvents CartridgeGame::take(const std::string &arg) {
    const auto item_id = resolve_item_ref(arg);
    if (!item_id) {
        return {{EventKind::narration, "Take what?"}};
    }
    const auto &item = world_.items.at(*item_id);
    const auto &loc_id = world_.player.current_location;
    if (!item_is_at(world_, *item_id, ItemHolder::location, loc_id) || item.hidden) {
        return {{EventKind::narration, "You don't see that here."}};
    }
    if (!item.takeable) {
        return {{EventKind::narration, "You can't take that."}};
    }
    if (auto moved = submit_world_action(actions::RelocateItem{
            .item_id = *item_id,
            .destination = ItemPosition{.holder = ItemHolder::player, .id = {}},
        });
        !moved) {
        return {{EventKind::warning, moved.error().reason}};
    }
    return {{EventKind::narration, "You take the " + item.name + "."}};
}

GameEvents CartridgeGame::drop(const std::string &arg) {
    const auto item_id = resolve_item_ref(arg);
    if (!item_id || !item_is_at(world_, *item_id, ItemHolder::player)) {
        return {{EventKind::narration, "You aren't carrying that."}};
    }
    const auto &item = world_.items.at(*item_id);
    const auto &loc_id = world_.player.current_location;
    if (auto moved = submit_world_action(actions::RelocateItem{
            .item_id = *item_id,
            .destination = ItemPosition{.holder = ItemHolder::location, .id = loc_id},
        });
        !moved) {
        return {{EventKind::warning, moved.error().reason}};
    }
    return {{EventKind::narration, "You drop the " + item.name + "."}};
}

GameEvents CartridgeGame::use(const std::string &arg) {
    static const std::regex on_pattern(R"((.+?)\s+on(?:/with)?\s+(.+))", std::regex::icase);
    static const std::regex with_pattern(R"((.+?)\s+with\s+(.+))", std::regex::icase);
    std::smatch match;
    if (!std::regex_match(arg, match, on_pattern) && !std::regex_match(arg, match, with_pattern)) {
        return {{EventKind::narration, "Use what on what?"}};
    }
    const auto item_id = resolve_item_ref(match[1].str());
    const std::string target = strip(lower(match[2].str()));
    if (!item_id || !item_is_at(world_, *item_id, ItemHolder::player)) {
        return {{EventKind::narration, "You aren't carrying that."}};
    }
    const auto &item = world_.items.at(*item_id);
    const auto &loc = world_.locations.at(world_.player.current_location);
    const std::string unlock = strip(lower(item.unlock_target));

    // unlock_target may be a direction or a destination location id.
    const auto target_exit = loc.exits.find(target);
    const bool target_matches =
        target == unlock || (target_exit != loc.exits.end() && target_exit->second == unlock);
    if (!unlock.empty() && target_matches) {
        const auto locked_now = loc.locked_directions();
        bool unlocked_any = false;
        for (const auto &entry : loc.locked_exits) {
            const auto exit_it = loc.exits.find(entry.direction);
            if (entry.direction == target || entry.direction == unlock ||
                (exit_it != loc.exits.end() && exit_it->second == unlock)) {
                unlocked_any = true;
            }
        }
        if (unlocked_any || locked_now.contains(target) || locked_now.contains(unlock)) {
            const std::string direction_to_clear = loc.exits.contains(target) ? target : unlock;
            if (auto result = submit_world_action(actions::UnlockExit{
                    .location_id = world_.player.current_location,
                    .direction = direction_to_clear,
                });
                !result) {
                return {{EventKind::warning, result.error().reason}};
            }
            return {{EventKind::narration, "You use the " + item.name + ". The way is unlocked."}};
        }
    }
    return {{EventKind::narration, "That doesn't work."}};
}

GameEvents CartridgeGame::talk(const std::string &arg) {
    const auto npc_id = resolve_npc_ref(arg);
    if (!npc_id) {
        return {{EventKind::narration, "Talk to whom?"}};
    }
    const auto &npc = world_.npcs.at(*npc_id);
    if (npc.state.current_location != world_.player.current_location) {
        return {{EventKind::narration, npc.identity.name + " isn't here."}};
    }
    if (auto begun = submit_world_action(actions::BeginConversation{.npc_id = *npc_id}); !begun) {
        return {{EventKind::warning, begun.error().reason}};
    }
    return {{EventKind::narration, "You begin talking with " + npc.identity.name + "."}};
}

} // namespace chronicle
