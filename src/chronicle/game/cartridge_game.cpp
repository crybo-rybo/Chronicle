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

void erase_value(std::vector<std::string> &values, const std::string &value) {
    if (const auto it = std::ranges::find(values, value); it != values.end()) {
        values.erase(it);
    }
}

std::optional<int> parse_int(const std::string &text) {
    int value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

bool truthy_param(const nlohmann::json &value, const bool fallback) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_string()) {
        const auto text = lower(value.get<std::string>());
        return text == "1" || text == "true" || text == "yes";
    }
    return fallback;
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
    : world_(std::move(world)), saves_(save_directory_for(world_, save_dir)) {}

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

GameEvents CartridgeGame::handle_player(const std::string &text) {
    const std::string raw = strip(text);
    if (raw.empty()) {
        return {};
    }
    if (phase_ == GamePhase::game_over) {
        return {{EventKind::narration, "The scenario has ended. Type quit to exit."}};
    }

    const std::string expanded = expand_alias(raw);
    const std::string lowered = strip(lower(expanded));

    if (phase_ == GamePhase::in_conversation) {
        if (EXIT_PHRASES.contains(lowered)) {
            phase_ = GamePhase::playing;
            active_npc_.reset();
            return {{EventKind::narration, "You end the conversation."}};
        }
        const auto [first, ignored] = split_command(lowered);
        if (HARD_CONVERSATION_COMMANDS.contains(first)) {
            return dispatch_playing(expanded);
        }
        if (BLOCKED_CONVERSATION_COMMANDS.contains(first)) {
            return {{EventKind::narration, "Finish the conversation first (say 'bye')."}};
        }
        // Free text is handled by the LLM path.
        return {};
    }

    return dispatch_playing(expanded);
}

bool CartridgeGame::wants_llm_turn(const std::string &text) const {
    if (phase_ != GamePhase::in_conversation || !active_npc_) {
        return false;
    }
    const std::string lowered = strip(lower(text));
    if (lowered.empty() || EXIT_PHRASES.contains(lowered)) {
        return false;
    }
    const auto [first, ignored] = split_command(lowered);
    return !HARD_CONVERSATION_COMMANDS.contains(first);
}

GameEvents CartridgeGame::after_turn() {
    GameEvents events;
    if (significant_ && phase_ != GamePhase::game_over) {
        world_.clock.turns_elapsed += 1;
        significant_ = false;
        if (world_.clock.time_expired()) {
            phase_ = GamePhase::game_over;
            events.push_back({EventKind::ending,
                              "Time has run out. The scenario ends without a clearer resolution."});
            return events;
        }
    }
    auto fired = evaluate_events();
    events.insert(events.end(), fired.begin(), fired.end());
    return events;
}

void CartridgeGame::save(const int slot) {
    nlohmann::json conversations = nlohmann::json::object();
    if (conversation_snapshot_) {
        conversations = conversation_snapshot_();
    }
    saves_.save(slot, world_, phase_, active_npc_, conversations);
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
        phase_ = GamePhase::game_over;
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
        const int slot = words.size() > 1 ? parse_int(words[1]).value_or(1) : 1;
        save(slot);
        return {{EventKind::narration, "Saved to slot " + std::to_string(slot) + "."}};
    }
    if (cmd == "load") {
        const int slot = words.size() > 1 ? parse_int(words[1]).value_or(1) : 1;
        return do_load(slot);
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
    world_ = std::move(loaded->world);
    phase_ = loaded->phase;
    active_npc_ = loaded->active_npc;
    if (conversation_restore_) {
        conversation_restore_(loaded->conversations);
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
    for (const auto &[iid, owner] : world_.item_owners) {
        const auto location_it = world_.item_locations.find(iid);
        const auto item_it = world_.items.find(iid);
        if (owner == "location" && location_it != world_.item_locations.end() &&
            location_it->second == loc_id && item_it != world_.items.end() &&
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
    if (world_.player.inventory.empty()) {
        return {{EventKind::narration, "You are carrying nothing."}};
    }
    std::string line = "You are carrying: ";
    bool first = true;
    for (const auto &iid : world_.player.inventory) {
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
    auto &loc = world_.locations.at(world_.player.current_location);
    const auto exit_it = loc.exits.find(direction);
    if (exit_it == loc.exits.end()) {
        return {{EventKind::narration, "You can't go that way."}};
    }
    if (loc.locked_directions().contains(direction)) {
        return {{EventKind::narration, "That way is locked."}};
    }
    world_.player.current_location = exit_it->second;
    significant_ = true;
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
    const auto owner_it = world_.item_owners.find(item_id);
    const std::string owner = owner_it == world_.item_owners.end() ? "" : owner_it->second;
    if (owner == "player" || contains(world_.player.inventory, item_id)) {
        return true;
    }
    const auto &here = world_.player.current_location;
    const auto location_it = world_.item_locations.find(item_id);
    return owner == "location" && location_it != world_.item_locations.end() &&
           location_it->second == here;
}

GameEvents CartridgeGame::take(const std::string &arg) {
    const auto item_id = resolve_item_ref(arg);
    if (!item_id) {
        return {{EventKind::narration, "Take what?"}};
    }
    const auto &item = world_.items.at(*item_id);
    const auto &loc_id = world_.player.current_location;
    const auto owner_it = world_.item_owners.find(*item_id);
    const auto location_it = world_.item_locations.find(*item_id);
    if (owner_it == world_.item_owners.end() || owner_it->second != "location" ||
        location_it == world_.item_locations.end() || location_it->second != loc_id) {
        return {{EventKind::narration, "You don't see that here."}};
    }
    if (!item.takeable) {
        return {{EventKind::narration, "You can't take that."}};
    }
    world_.item_owners[*item_id] = "player";
    world_.item_locations[*item_id] = loc_id;
    world_.player.inventory.push_back(*item_id);
    erase_value(world_.locations.at(loc_id).items, *item_id);
    significant_ = true;
    return {{EventKind::narration, "You take the " + item.name + "."}};
}

GameEvents CartridgeGame::drop(const std::string &arg) {
    const auto item_id = resolve_item_ref(arg);
    if (!item_id || !contains(world_.player.inventory, *item_id)) {
        return {{EventKind::narration, "You aren't carrying that."}};
    }
    const auto &item = world_.items.at(*item_id);
    const auto &loc_id = world_.player.current_location;
    erase_value(world_.player.inventory, *item_id);
    world_.item_owners[*item_id] = "location";
    world_.item_locations[*item_id] = loc_id;
    world_.locations.at(loc_id).items.push_back(*item_id);
    significant_ = true;
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
    if (!item_id || !contains(world_.player.inventory, *item_id)) {
        return {{EventKind::narration, "You aren't carrying that."}};
    }
    const auto &item = world_.items.at(*item_id);
    auto &loc = world_.locations.at(world_.player.current_location);
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
            std::erase_if(loc.locked_exits, [&](const LockedExitEntry &entry) {
                return entry.direction == direction_to_clear;
            });
            significant_ = true;
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
    auto &npc = world_.npcs.at(*npc_id);
    if (npc.state.current_location != world_.player.current_location) {
        return {{EventKind::narration, npc.identity.name + " isn't here."}};
    }
    phase_ = GamePhase::in_conversation;
    active_npc_ = *npc_id;
    npc.state.has_met_player = true;
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
        const ToolPolicy &policy;
        const decltype(check_item) &item_check;

        std::optional<std::string> operator()(const tools::Say &) const { return std::nullopt; }
        std::optional<std::string> operator()(const tools::GiveItem &args) const {
            if (auto issue = item_check(args.item_id)) {
                return issue;
            }
            if (!contains(npc.state.inventory, args.item_id)) {
                return "NPC does not hold that item";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::TakeItem &args) const {
            if (auto issue = item_check(args.item_id)) {
                return issue;
            }
            if (!contains(game.world_.player.inventory, args.item_id)) {
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
    return std::visit(Visitor{*this, npc, policy, check_item}, call);
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

GameEvents CartridgeGame::apply_npc_tool(const std::string &npc_id, const NpcToolCall &call) {
    auto &npc = world_.npcs.at(npc_id);

    struct Visitor {
        CartridgeGame &game;
        NpcData &npc;
        const std::string &npc_id;

        GameEvents operator()(const tools::Say &args) const {
            return {{EventKind::dialogue, npc.identity.name + ": \"" + strip(args.text) + "\""}};
        }
        GameEvents operator()(const tools::GiveItem &args) const {
            auto &world = game.world_;
            const auto &item = world.items.at(args.item_id);
            erase_value(npc.state.inventory, args.item_id);
            world.player.inventory.push_back(args.item_id);
            world.item_owners[args.item_id] = "player";
            world.item_locations[args.item_id] = world.player.current_location;
            game.significant_ = true;
            const auto ev = game.narrate("give_item_to_player",
                                         {{"npc", npc.identity.name}, {"item", item.name}});
            if (ev) {
                return {*ev};
            }
            return {
                {EventKind::narration, npc.identity.name + " gives you the " + item.name + "."}};
        }
        GameEvents operator()(const tools::TakeItem &args) const {
            auto &world = game.world_;
            const auto &item = world.items.at(args.item_id);
            erase_value(world.player.inventory, args.item_id);
            npc.state.inventory.push_back(args.item_id);
            world.item_owners[args.item_id] = npc_id;
            world.item_locations[args.item_id] = npc.state.current_location;
            game.significant_ = true;
            const auto ev = game.narrate("take_item_from_player",
                                         {{"npc", npc.identity.name}, {"item", item.name}});
            if (ev) {
                return {*ev};
            }
            return {{EventKind::narration, npc.identity.name + " takes the " + item.name + "."}};
        }
        GameEvents operator()(const tools::UpdateMood &args) const {
            const std::string mood = tools::to_string(args.mood);
            npc.state.mood = mood;
            const auto ev =
                game.narrate("update_npc_mood", {{"npc", npc.identity.name}, {"mood", mood}});
            return ev ? GameEvents{*ev} : GameEvents{};
        }
        GameEvents operator()(const tools::UpdateTrust &args) const {
            npc.state.trust_toward_player =
                std::clamp(npc.state.trust_toward_player + args.delta, 0, 100);
            if (!npc.identity.secret.empty() &&
                npc.state.trust_toward_player >= npc.identity.trust_reveal_threshold) {
                npc.state.secret_revealed = true;
            }
            const auto ev = game.narrate("update_npc_trust", {{"npc", npc.identity.name}});
            return ev ? GameEvents{*ev} : GameEvents{};
        }
        GameEvents operator()(const tools::MoveSelf &args) const {
            auto &world = game.world_;
            npc.state.current_location = args.location_id;
            game.significant_ = true;
            if (game.active_npc_ == npc_id && args.location_id != world.player.current_location) {
                game.phase_ = GamePhase::playing;
                game.active_npc_.reset();
            }
            const auto ev =
                game.narrate("move_npc", {{"npc", npc.identity.name},
                                          {"location", world.locations.at(args.location_id).name}});
            return ev ? GameEvents{*ev} : GameEvents{};
        }
        GameEvents operator()(const tools::RevealKnowledge &args) const {
            auto &world = game.world_;
            const auto &fact = world.facts.at(args.fact_id);
            world.revealed_facts.insert(args.fact_id);
            game.significant_ = true;
            GameEvents events;
            if (const auto ev = game.narrate("reveal_knowledge", {})) {
                events.push_back(*ev);
            }
            events.push_back({EventKind::knowledge, fact.text});
            return events;
        }
        GameEvents operator()(const tools::Remember &args) const {
            npc.state.memories.push_back(MemoryEntry{
                .timestamp = game.world_.clock.period_name(),
                .type = "observation",
                .summary = strip(args.summary),
                .importance = args.importance,
                .related_npc = "",
                .related_item = "",
            });
            const auto ev = game.narrate("add_memory", {});
            return ev ? GameEvents{*ev} : GameEvents{};
        }
        GameEvents operator()(const tools::SetFlag &args) const {
            game.world_.flags[args.flag_id] = args.value;
            game.significant_ = true;
            const auto ev = game.narrate("set_flag", {});
            return ev ? GameEvents{*ev} : GameEvents{};
        }
        GameEvents operator()(const tools::InspectItem &args) const {
            const auto &item = game.world_.items.at(args.item_id);
            return {{EventKind::tool_result, "[inspect] " + item.name + ": " + item.description}};
        }
    };
    return std::visit(Visitor{*this, npc, npc_id}, call);
}

// --- scripted events -----------------------------------------------------

GameEvents CartridgeGame::evaluate_events() {
    GameEvents events;
    std::vector<EventActionData> pending;
    for (auto &[event_id, trigger] : world_.events) {
        if (trigger.once && trigger.fired) {
            continue;
        }
        const bool met = std::ranges::all_of(
            trigger.conditions, [&](const ConditionData &cond) { return condition_met(cond); });
        if (!met) {
            continue;
        }
        trigger.fired = true;
        pending.insert(pending.end(), trigger.actions.begin(), trigger.actions.end());
    }
    for (const auto &action : pending) {
        auto applied = apply_event_action(action);
        events.insert(events.end(), applied.begin(), applied.end());
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
        return !args.empty() && contains(world_.player.inventory, args[0]);
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
        phase_ = GamePhase::game_over;
        active_npc_.reset();
        const std::string text = str_param("text");
        return {{EventKind::ending, text.empty() ? "The scenario ends." : text}};
    }
    if (action.type == "move_npc") {
        const auto npc_it = world_.npcs.find(str_param("npc_id"));
        const auto loc_it = world_.locations.find(str_param("location_id"));
        if (npc_it == world_.npcs.end() || loc_it == world_.locations.end()) {
            return {};
        }
        npc_it->second.state.current_location = loc_it->first;
        return {{EventKind::narration,
                 npc_it->second.identity.name + " moves to " + loc_it->second.name + "."}};
    }
    if (action.type == "set_flag") {
        const std::string flag_id = str_param("flag_id");
        if (flag_id.empty()) {
            return {};
        }
        const auto value_it = params.find("value");
        world_.flags[flag_id] = value_it == params.end() ? true : truthy_param(*value_it, true);
        return {};
    }
    if (action.type == "spawn_item") {
        const std::string item_id = str_param("item_id");
        const auto loc_it = world_.locations.find(str_param("location_id"));
        if (item_id.empty() || loc_it == world_.locations.end()) {
            return {};
        }
        world_.item_owners[item_id] = "location";
        world_.item_locations[item_id] = loc_it->first;
        if (!contains(loc_it->second.items, item_id)) {
            loc_it->second.items.push_back(item_id);
        }
        return {{EventKind::narration, "Something new appears in " + loc_it->second.name + "."}};
    }
    return {};
}

} // namespace chronicle
