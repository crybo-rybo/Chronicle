#include "chronicle/game/cartridge_game.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <regex>
#include <sstream>

#include "chronicle/cartridge/loader.hpp"
#include "chronicle/cartridge/validator.hpp"

namespace chronicle {

namespace {

using tools::NpcToolCall;

const std::set<std::string> EXIT_PHRASES = {"bye", "goodbye", "leave", "exit conversation"};
const std::set<std::string> HARD_CONVERSATION_COMMANDS = {"look", "inventory", "help",
                                                          "quit", "save",      "load"};
const std::set<std::string> BLOCKED_CONVERSATION_COMMANDS = {"go",   "examine", "take",
                                                             "drop", "use",     "talk"};

std::string lower(std::string text) {
    std::ranges::transform(text, text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string strip(const std::string &text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::vector<std::string> split_words(const std::string &text) {
    std::istringstream stream(text);
    std::vector<std::string> words;
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

// First word, and the remainder of the original-case text after it.
std::pair<std::string, std::string> split_command(const std::string &text) {
    const auto stripped = strip(text);
    const auto space = stripped.find_first_of(" \t");
    if (space == std::string::npos) {
        return {lower(stripped), ""};
    }
    return {lower(stripped.substr(0, space)), strip(stripped.substr(space + 1))};
}

bool contains(const std::vector<std::string> &haystack, const std::string &needle) {
    return std::ranges::find(haystack, needle) != haystack.end();
}

std::optional<int> parse_int(const std::string &text) {
    int value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::string format_template(std::string text, const std::map<std::string, std::string> &args) {
    for (const auto &[key, value] : args) {
        const std::string placeholder = "{" + key + "}";
        std::size_t pos = 0;
        while ((pos = text.find(placeholder, pos)) != std::string::npos) {
            text.replace(pos, placeholder.size(), value);
            pos += value.size();
        }
    }
    return text;
}

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

// --- the action gate -----------------------------------------------------

ToolOutcome CartridgeGame::submit_npc_tool(const std::string &npc_id, const NpcToolCall &call) {
    if (auto reason = validate_npc_tool(npc_id, call)) {
        return std::unexpected(ToolRejection{std::move(*reason)});
    }
    return apply_npc_tool(npc_id, call);
}

std::optional<std::string> CartridgeGame::validate_npc_tool(const std::string &npc_id,
                                                            const NpcToolCall &call) const {
    const auto npc_it = world_.npcs.find(npc_id);
    if (npc_it == world_.npcs.end()) {
        return "Unknown NPC";
    }
    const auto &npc = npc_it->second;
    const auto &policy = npc.identity.tool_policy;
    const std::string name(tools::tool_name(call));
    if (!contains(policy.allowed_tools, name)) {
        return "Tool '" + name + "' not allowed for " + npc.identity.name;
    }

    const auto check_item = [&](const std::string &item_id) -> std::optional<std::string> {
        if (!policy.allowed_items.empty() && !contains(policy.allowed_items, item_id)) {
            return "Item '" + item_id + "' not allowed";
        }
        if (!world_.items.contains(item_id)) {
            return "Unknown item '" + item_id + "'";
        }
        return std::nullopt;
    };

    struct Visitor {
        const CartridgeGame &game;
        const NpcData &npc;
        const std::string &npc_id;
        const ToolPolicy &policy;
        const decltype(check_item) &item_check;

        std::optional<std::string> operator()(const tools::Say &) const { return std::nullopt; }
        std::optional<std::string> operator()(const tools::GiveItem &args) const {
            if (auto issue = item_check(args.item_id)) {
                return issue;
            }
            if (!item_is_at(game.world_, args.item_id, ItemHolder::npc, npc_id)) {
                return "NPC does not hold that item";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::TakeItem &args) const {
            if (auto issue = item_check(args.item_id)) {
                return issue;
            }
            if (!item_is_at(game.world_, args.item_id, ItemHolder::player)) {
                return "Player does not hold that item";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::UpdateMood &) const {
            return std::nullopt; // Mood validity is enforced by the enum type.
        }
        std::optional<std::string> operator()(const tools::UpdateTrust &) const {
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::MoveSelf &args) const {
            if (!game.world_.locations.contains(args.location_id)) {
                return "Unknown location";
            }
            if (!policy.allowed_locations.empty() &&
                !contains(policy.allowed_locations, args.location_id)) {
                return "Location not allowed";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::RevealKnowledge &args) const {
            if (!contains(npc.identity.knowledge, args.fact_id)) {
                return "NPC does not know that fact";
            }
            if (!policy.allowed_facts.empty() && !contains(policy.allowed_facts, args.fact_id)) {
                return "Fact not allowed";
            }
            if (!game.world_.facts.contains(args.fact_id)) {
                return "Unknown fact";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::Remember &args) const {
            if (strip(args.summary).empty()) {
                return "Memory summary required";
            }
            if (args.importance < 1 || args.importance > 10) {
                return "Memory importance must be between 1 and 10";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::SetFlag &args) const {
            if (!policy.allowed_flags.empty() && !contains(policy.allowed_flags, args.flag_id)) {
                return "Flag not allowed";
            }
            if (!game.world_.flags.contains(args.flag_id) &&
                !game.world_.flag_meta.contains(args.flag_id)) {
                return "Unknown flag";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::InspectItem &args) const {
            return item_check(args.item_id);
        }
    };
    return std::visit(Visitor{*this, npc, npc_id, policy, check_item}, call);
}

std::optional<GameEvent>
CartridgeGame::narrate(const std::string &key,
                       const std::map<std::string, std::string> &args) const {
    const auto it = world_.config.mutation_narration_templates.find(key);
    if (it == world_.config.mutation_narration_templates.end() || it->second.empty()) {
        return std::nullopt;
    }
    return GameEvent{EventKind::narration, format_template(it->second, args)};
}

ToolOutcome CartridgeGame::apply_npc_tool(const std::string &npc_id, const NpcToolCall &call) {
    const auto &npc = world_.npcs.at(npc_id);

    const auto submit = [this](actions::WorldAction action) -> std::optional<ToolRejection> {
        auto outcome = submit_world_action(std::move(action));
        if (!outcome) {
            return ToolRejection{.reason = outcome.error().reason};
        }
        return std::nullopt;
    };

    struct Visitor {
        CartridgeGame &game;
        const NpcData &npc;
        const std::string &npc_id;
        const decltype(submit) &submit_action;

        ToolOutcome operator()(const tools::Say &args) const {
            return GameEvents{
                {EventKind::dialogue, npc.identity.name + ": \"" + strip(args.text) + "\""}};
        }
        ToolOutcome operator()(const tools::GiveItem &args) const {
            const auto &item = game.world_.items.at(args.item_id);
            if (auto rejected = submit_action(actions::RelocateItem{
                    .item_id = args.item_id,
                    .destination = ItemPosition{.holder = ItemHolder::player, .id = {}},
                })) {
                return std::unexpected(std::move(*rejected));
            }
            const auto event = game.narrate("give_item_to_player",
                                            {{"npc", npc.identity.name}, {"item", item.name}});
            return event ? GameEvents{*event}
                         : GameEvents{{EventKind::narration,
                                       npc.identity.name + " gives you the " + item.name + "."}};
        }
        ToolOutcome operator()(const tools::TakeItem &args) const {
            const auto &item = game.world_.items.at(args.item_id);
            if (auto rejected = submit_action(actions::RelocateItem{
                    .item_id = args.item_id,
                    .destination = ItemPosition{.holder = ItemHolder::npc, .id = npc_id},
                })) {
                return std::unexpected(std::move(*rejected));
            }
            const auto event = game.narrate("take_item_from_player",
                                            {{"npc", npc.identity.name}, {"item", item.name}});
            return event ? GameEvents{*event}
                         : GameEvents{{EventKind::narration,
                                       npc.identity.name + " takes the " + item.name + "."}};
        }
        ToolOutcome operator()(const tools::UpdateMood &args) const {
            const std::string mood = tools::to_string(args.mood);
            if (auto rejected =
                    submit_action(actions::UpdateNpcMood{.npc_id = npc_id, .mood = mood})) {
                return std::unexpected(std::move(*rejected));
            }
            const auto event =
                game.narrate("update_npc_mood", {{"npc", npc.identity.name}, {"mood", mood}});
            return event ? GameEvents{*event} : GameEvents{};
        }
        ToolOutcome operator()(const tools::UpdateTrust &args) const {
            if (auto rejected =
                    submit_action(actions::AdjustNpcTrust{.npc_id = npc_id, .delta = args.delta})) {
                return std::unexpected(std::move(*rejected));
            }
            const auto event = game.narrate("update_npc_trust", {{"npc", npc.identity.name}});
            return event ? GameEvents{*event} : GameEvents{};
        }
        ToolOutcome operator()(const tools::MoveSelf &args) const {
            const std::string location_name = game.world_.locations.at(args.location_id).name;
            if (auto rejected = submit_action(actions::MoveNpc{
                    .npc_id = npc_id, .destination = args.location_id, .significant = true})) {
                return std::unexpected(std::move(*rejected));
            }
            const auto event =
                game.narrate("move_npc", {{"npc", npc.identity.name}, {"location", location_name}});
            return event ? GameEvents{*event} : GameEvents{};
        }
        ToolOutcome operator()(const tools::RevealKnowledge &args) const {
            const std::string fact_text = game.world_.facts.at(args.fact_id).text;
            if (auto rejected = submit_action(actions::RevealFact{.fact_id = args.fact_id})) {
                return std::unexpected(std::move(*rejected));
            }
            GameEvents events;
            if (const auto event = game.narrate("reveal_knowledge", {})) {
                events.push_back(*event);
            }
            events.push_back({EventKind::knowledge, fact_text});
            return events;
        }
        ToolOutcome operator()(const tools::Remember &args) const {
            if (auto rejected = submit_action(actions::AddMemory{
                    .npc_id = npc_id,
                    .memory =
                        MemoryEntry{
                            .timestamp = game.world_.clock.period_name(),
                            .type = "observation",
                            .summary = strip(args.summary),
                            .importance = args.importance,
                            .related_npc = "",
                            .related_item = "",
                        },
                })) {
                return std::unexpected(std::move(*rejected));
            }
            const auto event = game.narrate("add_memory", {});
            return event ? GameEvents{*event} : GameEvents{};
        }
        ToolOutcome operator()(const tools::SetFlag &args) const {
            if (auto rejected =
                    submit_action(actions::SetFlag{.flag_id = args.flag_id, .value = args.value})) {
                return std::unexpected(std::move(*rejected));
            }
            const auto event = game.narrate("set_flag", {});
            return event ? GameEvents{*event} : GameEvents{};
        }
        ToolOutcome operator()(const tools::InspectItem &args) const {
            const auto &item = game.world_.items.at(args.item_id);
            return GameEvents{
                {EventKind::tool_result, "[inspect] " + item.name + ": " + item.description}};
        }
    };
    return std::visit(Visitor{*this, npc, npc_id, submit}, call);
}

// --- scripted events -----------------------------------------------------

GameEvents CartridgeGame::evaluate_events() {
    GameEvents events;
    std::vector<std::pair<std::string, std::vector<EventActionData>>> pending;
    for (const auto &[event_id, trigger] : world_.events) {
        if (trigger.once && trigger.fired) {
            continue;
        }
        const bool met = std::ranges::all_of(
            trigger.conditions, [&](const ConditionData &cond) { return condition_met(cond); });
        if (!met) {
            continue;
        }
        pending.emplace_back(event_id, trigger.actions);
    }
    for (const auto &[event_id, event_actions] : pending) {
        if (auto marked = submit_world_action(actions::MarkEventFired{.event_id = event_id});
            !marked) {
            events.push_back({EventKind::warning, marked.error().reason});
            continue;
        }
        for (const auto &action : event_actions) {
            auto applied = apply_event_action(action);
            events.insert(events.end(), applied.begin(), applied.end());
        }
    }
    return events;
}

bool CartridgeGame::condition_met(const ConditionData &cond) const {
    const auto &args = cond.args;
    if (cond.type == "clock_is") {
        return !args.empty() && world_.clock.period_name() == args[0];
    }
    if (cond.type == "player_at") {
        return !args.empty() && world_.player.current_location == args[0];
    }
    if (cond.type == "flag_set") {
        if (args.size() < 2) {
            return false;
        }
        const std::string want_text = lower(args[1]);
        const bool want = want_text == "1" || want_text == "true" || want_text == "yes";
        const auto it = world_.flags.find(args[0]);
        const bool value = it != world_.flags.end() && it->second;
        return value == want;
    }
    if (cond.type == "npc_trust_ge") {
        if (args.size() < 2) {
            return false;
        }
        const auto npc = world_.npcs.find(args[0]);
        const auto threshold = parse_int(args[1]);
        return npc != world_.npcs.end() && threshold &&
               npc->second.state.trust_toward_player >= *threshold;
    }
    if (cond.type == "npc_at") {
        if (args.size() < 2) {
            return false;
        }
        const auto npc = world_.npcs.find(args[0]);
        return npc != world_.npcs.end() && npc->second.state.current_location == args[1];
    }
    if (cond.type == "item_in_player_inv") {
        return !args.empty() && item_is_at(world_, args[0], ItemHolder::player);
    }
    if (cond.type == "turn_ge") {
        if (args.empty()) {
            return false;
        }
        const auto threshold = parse_int(args[0]);
        return threshold && world_.clock.turns_elapsed >= *threshold;
    }
    return false;
}

GameEvents CartridgeGame::apply_event_action(const EventActionData &action) {
    const auto &params = action.params;
    const auto str_param = [&](const char *key) -> std::string {
        const auto it = params.find(key);
        return it != params.end() && it->is_string() ? it->get<std::string>() : "";
    };

    if (action.type == "narrate") {
        return {{EventKind::narration, str_param("text")}};
    }
    if (action.type == "end_game") {
        if (auto ended = submit_world_action(actions::EndGame{}); !ended) {
            return {{EventKind::warning, ended.error().reason}};
        }
        const std::string text = str_param("text");
        return {{EventKind::ending, text.empty() ? "The scenario ends." : text}};
    }
    if (action.type == "move_npc") {
        const auto npc_it = world_.npcs.find(str_param("npc_id"));
        const auto loc_it = world_.locations.find(str_param("location_id"));
        if (npc_it == world_.npcs.end() || loc_it == world_.locations.end()) {
            return {};
        }
        if (auto moved = submit_world_action(actions::MoveNpc{
                .npc_id = npc_it->first, .destination = loc_it->first, .significant = false});
            !moved) {
            return {{EventKind::warning, moved.error().reason}};
        }
        return {{EventKind::narration,
                 npc_it->second.identity.name + " moves to " + loc_it->second.name + "."}};
    }
    if (action.type == "set_flag") {
        const std::string flag_id = str_param("flag_id");
        if (flag_id.empty()) {
            return {};
        }
        const auto value_it = params.find("value");
        if (value_it == params.end() || !value_it->is_boolean()) {
            return {{EventKind::warning, "Event set_flag value is not boolean"}};
        }
        if (auto set = submit_world_action(actions::SetFlag{
                .flag_id = flag_id, .value = value_it->get<bool>(), .significant = false});
            !set) {
            return {{EventKind::warning, set.error().reason}};
        }
        return {};
    }
    if (action.type == "spawn_item") {
        const std::string item_id = str_param("item_id");
        const auto loc_it = world_.locations.find(str_param("location_id"));
        if (item_id.empty() || loc_it == world_.locations.end()) {
            return {};
        }
        if (auto moved = submit_world_action(actions::RelocateItem{
                .item_id = item_id,
                .destination = ItemPosition{.holder = ItemHolder::location, .id = loc_it->first},
                .significant = false,
            });
            !moved) {
            return {{EventKind::warning, moved.error().reason}};
        }
        return {{EventKind::narration, "Something new appears in " + loc_it->second.name + "."}};
    }
    return {};
}

} // namespace chronicle
