#include "chronicle/game/cartridge_game.hpp"

#include <algorithm>
#include <regex>

#include "chronicle/cartridge/loader.hpp"
#include "chronicle/game/player_verbs.hpp"
#include "chronicle/game/text_utils.hpp"
#include "chronicle/paths.hpp"

namespace chronicle {

namespace {

using game_detail::best_matches;
using game_detail::Fit;
using game_detail::fit_of;
using game_detail::lower;
using game_detail::names;
using game_detail::Noun;
using game_detail::parse_int;
using game_detail::split_command;
using game_detail::split_words;
using game_detail::strip;

// A spoken line is a command only when all of it is one: "quit", "save 2", "bye!".
// Anything longer, such as "quit lying to me" or "look at this", is something to say.
bool is_whole_command(const std::vector<std::string> &words) {
    if (words.size() == 1) {
        const auto &word = words.front();
        return word == "look" || word == "inventory" || word == "help" || word == "quit" ||
               word == "q" || word == "save" || word == "load";
    }
    return words.size() == 2 && (words[0] == "save" || words[0] == "load") && parse_int(words[1]);
}

struct UseCommand {
    std::string item;
    std::string target;
};

std::optional<UseCommand> parse_use(const std::string &arg) {
    static const std::regex pattern(R"((.+?)\s+(?:on|with|on/with)\s+(.+))", std::regex::icase);
    std::smatch match;
    if (!std::regex_match(arg, match, pattern)) {
        return std::nullopt;
    }
    return UseCommand{.item = match[1].str(), .target = strip(lower(match[2].str()))};
}

// "talk to the keeper" and "go to the study" read as they are meant.
std::string without_preposition(const std::string &text) {
    const std::string lowered = lower(text);
    for (const std::string_view preposition : {"to ", "with "}) {
        if (lowered.starts_with(preposition)) {
            return strip(text.substr(preposition.size()));
        }
    }
    return text;
}

GameEvents which_of(const std::vector<Noun> &matches) {
    std::string question = "Which do you mean: ";
    for (std::size_t i = 0; i < matches.size(); ++i) {
        question += i == 0 ? "" : (i + 1 == matches.size() ? " or " : ", ");
        question += matches[i].name;
    }
    return {{EventKind::narration, question + "?"}};
}

std::vector<Noun> item_nouns(const WorldState &world, const std::vector<std::string> &item_ids) {
    std::vector<Noun> nouns;
    for (const auto &item_id : item_ids) {
        if (const auto item = world.items.find(item_id); item != world.items.end()) {
            nouns.push_back({.id = item_id, .name = item->second.name});
        }
    }
    return nouns;
}

std::string without_closing_punctuation(std::string text) {
    text.resize(text.find_last_not_of(".!") + 1);
    return text;
}

} // namespace

CartridgeGame::CartridgeGame(const std::filesystem::path &package_dir,
                             std::optional<std::filesystem::path> save_dir)
    : CartridgeGame(load_package(package_dir), std::move(save_dir)) {}

CartridgeGame::CartridgeGame(WorldState world, std::optional<std::filesystem::path> save_dir)
    : world_(std::move(world)), action_gate_(world_, phase_, active_npc_, significant_),
      saves_(save_dir.value_or(save_directory(world_.manifest.id)), world_) {}

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
    if (phase_ == GamePhase::in_conversation) {
        return dispatch_conversation(raw, expanded);
    }
    return {.events = dispatch_playing(expanded), .npc_turn = std::nullopt};
}

PlayerDispatch CartridgeGame::dispatch_conversation(const std::string &raw,
                                                    const std::string &expanded) {
    const std::string lowered = lower(expanded);
    // An alias for "exit" must not take "exit conversation" away.
    if (is_exit_phrase(without_closing_punctuation(lower(raw))) ||
        is_exit_phrase(without_closing_punctuation(lowered))) {
        (void)submit_world_action(actions::EndConversation{});
        return {.events = {{EventKind::narration, "You end the conversation."}},
                .npc_turn = std::nullopt};
    }
    if (is_whole_command(split_words(lowered))) {
        return {.events = dispatch_playing(expanded), .npc_turn = std::nullopt};
    }
    if (names_something_here(expanded)) {
        return {.events = {{EventKind::narration, "Finish the conversation first (say 'bye')."}},
                .npc_turn = std::nullopt};
    }
    return {.events = {}, .npc_turn = NpcTurnRequest{.npc_id = *active_npc_, .player_text = raw}};
}

bool CartridgeGame::names_something_here(const std::string &line) const {
    const auto [verb, arg] = split_command(line);
    // Part of a name is too little: "drop the act" is not about a cactus.
    const auto one_of = [](const std::string &text, const std::vector<Noun> &nouns) {
        return std::ranges::any_of(
            nouns, [&](const Noun &noun) { return fit_of(text, noun) >= Fit::words_of_name; });
    };
    if (verb == "go") {
        return !exits_named(arg).empty();
    }
    if (verb == "take") {
        return one_of(arg, item_nouns(world_, items_lying_here()));
    }
    if (verb == "examine") {
        return one_of(arg, item_nouns(world_, items_accessible()));
    }
    if (verb == "drop") {
        return one_of(arg, item_nouns(world_, items_carried()));
    }
    if (verb == "use") {
        const auto used = parse_use(arg);
        return used && one_of(used->item, item_nouns(world_, items_carried()));
    }
    return verb == "talk" && one_of(without_preposition(arg), npcs_here());
}

std::vector<Noun> CartridgeGame::npcs_here() const {
    std::vector<Noun> here;
    for (const auto &[npc_id, npc] : world_.npcs) {
        if (npc.state.current_location == world_.player.current_location) {
            here.push_back({.id = npc_id, .name = npc.identity.name});
        }
    }
    return here;
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
    const bool turn_passed = significant_ && phase_ != GamePhase::game_over;
    if (turn_passed) {
        (void)submit_world_action(actions::AdvanceClock{});
    }
    GameEvents events = evaluate_events(turn_passed);
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
        return "In conversation: speak freely, or enter only bye, look, inventory, save [n], "
               "load [n], help, or quit.";
    }
    return "Commands: go <dir>, look, examine <item>, take/drop <item>, "
           "use <item> on <target>, talk <npc>, inventory, save [n], load [n], help, quit";
}

// --- player commands -----------------------------------------------------

std::string CartridgeGame::expand_alias(const std::string &text) const {
    const auto [first, rest] = split_command(text);
    const auto it = world_.config.verb_aliases.find(first);
    // An alias can add a word, never take one over: one for "quit" would leave a game
    // nobody can leave.
    if (it == world_.config.verb_aliases.end() || is_player_verb(first) || is_exit_phrase(first)) {
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
        return go(arg);
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

std::vector<std::string> CartridgeGame::items_lying_here() const {
    auto item_ids = items_at(world_, ItemHolder::location, world_.player.current_location);
    std::erase_if(item_ids, [&](const std::string &item_id) {
        const auto item = world_.items.find(item_id);
        return item == world_.items.end() || item->second.hidden;
    });
    return item_ids;
}

std::vector<std::string> CartridgeGame::items_carried() const {
    return items_at(world_, ItemHolder::player);
}

std::vector<std::string> CartridgeGame::items_accessible() const {
    auto item_ids = items_carried();
    const auto lying_here = items_lying_here();
    item_ids.insert(item_ids.end(), lying_here.begin(), lying_here.end());
    return item_ids;
}

std::vector<std::string> CartridgeGame::exits_named(const std::string &text) const {
    const auto &exits = world_.locations.at(world_.player.current_location).exits;
    const auto in_direction = [&](const std::string &wanted) {
        return std::ranges::find_if(
            exits, [&](const auto &exit) { return names(wanted, exit.first, exit.first); });
    };
    const std::string place = without_preposition(text);
    for (const auto &wanted : {text, place}) {
        if (const auto exit = in_direction(wanted); exit != exits.end()) {
            return {exit->first};
        }
    }
    std::vector<std::string> directions;
    for (const auto &[direction, destination] : exits) {
        if (names(place, destination, world_.locations.at(destination).name)) {
            directions.push_back(direction);
        }
    }
    return directions;
}

GameEvents CartridgeGame::go(const std::string &arg) {
    if (arg.empty()) {
        return {{EventKind::narration, "Go where?"}};
    }
    auto directions = exits_named(arg);
    if (directions.empty()) {
        // "go north now"
        const auto first = split_words(arg).front();
        directions = exits_named(first);
        std::erase_if(directions, [&](const std::string &direction) {
            return !names(first, direction, direction);
        });
    }
    if (directions.empty()) {
        return {{EventKind::narration, "You can't go that way."}};
    }
    const auto locked = world_.locations.at(world_.player.current_location).locked_directions();
    const auto open = std::ranges::find_if(
        directions, [&](const std::string &direction) { return !locked.contains(direction); });
    if (open == directions.end()) {
        return {{EventKind::narration, "That way is locked."}};
    }
    if (auto moved = submit_world_action(actions::MovePlayer{.direction = *open}); !moved) {
        return {{EventKind::warning, moved.error().reason}};
    }
    return look();
}

GameEvents CartridgeGame::examine(const std::string &arg) const {
    if (arg.empty()) {
        return {{EventKind::narration, "Examine what?"}};
    }
    const auto matches = best_matches(arg, item_nouns(world_, items_accessible()));
    if (matches.empty()) {
        return {{EventKind::narration, "You don't see that here."}};
    }
    if (matches.size() > 1) {
        return which_of(matches);
    }
    return {{EventKind::narration, item_text(world_.items.at(matches.front().id))}};
}

GameEvents CartridgeGame::take(const std::string &arg) {
    if (arg.empty()) {
        return {{EventKind::narration, "Take what?"}};
    }
    const auto matches = best_matches(arg, item_nouns(world_, items_lying_here()));
    if (matches.empty()) {
        const bool carried = !best_matches(arg, item_nouns(world_, items_carried())).empty();
        return {{EventKind::narration,
                 carried ? "You already have that." : "You don't see that here."}};
    }
    if (matches.size() > 1) {
        return which_of(matches);
    }
    const auto &[item_id, name] = matches.front();
    if (!world_.items.at(item_id).takeable) {
        return {{EventKind::narration, "You can't take that."}};
    }
    if (auto moved = submit_world_action(actions::RelocateItem{
            .item_id = item_id,
            .destination = ItemPosition{.holder = ItemHolder::player, .id = {}},
        });
        !moved) {
        return {{EventKind::warning, moved.error().reason}};
    }
    return {{EventKind::narration, "You take the " + name + "."}};
}

GameEvents CartridgeGame::drop(const std::string &arg) {
    if (arg.empty()) {
        return {{EventKind::narration, "Drop what?"}};
    }
    const auto matches = best_matches(arg, item_nouns(world_, items_carried()));
    if (matches.empty()) {
        return {{EventKind::narration, "You aren't carrying that."}};
    }
    if (matches.size() > 1) {
        return which_of(matches);
    }
    const auto &[item_id, name] = matches.front();
    if (auto moved = submit_world_action(actions::RelocateItem{
            .item_id = item_id,
            .destination =
                ItemPosition{.holder = ItemHolder::location, .id = world_.player.current_location},
        });
        !moved) {
        return {{EventKind::warning, moved.error().reason}};
    }
    return {{EventKind::narration, "You drop the " + name + "."}};
}

GameEvents CartridgeGame::use(const std::string &arg) {
    const auto used = parse_use(arg);
    if (!used) {
        return {{EventKind::narration, "Use what on what?"}};
    }
    const auto matches = best_matches(used->item, item_nouns(world_, items_carried()));
    if (matches.empty()) {
        return {{EventKind::narration, "You aren't carrying that."}};
    }
    if (matches.size() > 1) {
        return which_of(matches);
    }
    const auto &[item_id, name] = matches.front();

    // unlock_target may be a direction or a destination location id.
    const std::string unlocks = strip(lower(world_.items.at(item_id).unlock_target));
    const auto location_id = world_.player.current_location;
    const auto &location = world_.locations.at(location_id);
    auto directions = exits_named(used->target);
    std::erase_if(directions, [&](const std::string &direction) {
        return unlocks.empty() ||
               (unlocks != lower(direction) && unlocks != lower(location.exits.at(direction)));
    });
    if (directions.empty()) {
        return {{EventKind::narration, "That doesn't work."}};
    }
    const auto locked = location.locked_directions();
    std::erase_if(directions,
                  [&](const std::string &direction) { return !locked.contains(direction); });
    if (directions.empty()) {
        return {{EventKind::narration, "It isn't locked."}};
    }
    for (const auto &direction : directions) {
        if (auto unlocked = submit_world_action(
                actions::UnlockExit{.location_id = location_id, .direction = direction});
            !unlocked) {
            return {{EventKind::warning, unlocked.error().reason}};
        }
    }
    return {{EventKind::narration, "You use the " + name + ". The way is unlocked."}};
}

GameEvents CartridgeGame::talk(const std::string &arg) {
    const auto here = npcs_here();
    std::vector<Noun> elsewhere;
    for (const auto &[npc_id, npc] : world_.npcs) {
        if (npc.state.current_location != world_.player.current_location) {
            elsewhere.push_back({.id = npc_id, .name = npc.identity.name});
        }
    }
    const std::string who = without_preposition(arg);
    const auto matches = best_matches(who, here);
    if (matches.size() > 1) {
        return which_of(matches);
    }
    if (matches.empty()) {
        const auto absent = best_matches(who, elsewhere);
        return {{EventKind::narration,
                 absent.empty() ? "Talk to whom?" : absent.front().name + " isn't here."}};
    }
    const auto &[npc_id, name] = matches.front();
    if (auto begun = submit_world_action(actions::BeginConversation{.npc_id = npc_id}); !begun) {
        return {{EventKind::warning, begun.error().reason}};
    }
    return {{EventKind::narration, "You begin talking with " + name + "."}};
}

} // namespace chronicle
