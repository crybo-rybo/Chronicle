#include "chronicle/game/cartridge_game.hpp"

#include "chronicle/game/text_utils.hpp"

namespace chronicle {

namespace {

using game_detail::contains;
using game_detail::dialogue_line;
using game_detail::format_template;
using game_detail::single_line;
using tools::NpcToolCall;

} // namespace

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

    // A scope list names everything its tools may touch; an empty one, nothing.
    const auto check_item = [&](const std::string &item_id) -> std::optional<std::string> {
        if (!world_.items.contains(item_id)) {
            return "Unknown item '" + item_id + "'";
        }
        if (!contains(policy.allowed_items, item_id)) {
            return "Item '" + item_id + "' not allowed";
        }
        return std::nullopt;
    };
    // An NPC that has left cannot speak to the player or hand things over.
    const auto check_present = [&]() -> std::optional<std::string> {
        if (npc.state.current_location != world_.player.current_location) {
            return "You are no longer with the player";
        }
        return std::nullopt;
    };

    struct Visitor {
        const CartridgeGame &game;
        const NpcData &npc;
        const std::string &npc_id;
        const ToolPolicy &policy;
        const decltype(check_item) &item_check;
        const decltype(check_present) &present_check;

        std::optional<std::string> operator()(const tools::Say &args) const {
            if (single_line(args.text).empty()) {
                return "There is nothing to say in an empty text";
            }
            return present_check();
        }
        std::optional<std::string> operator()(const tools::GiveItem &args) const {
            if (auto issue = present_check()) {
                return issue;
            }
            if (auto issue = item_check(args.item_id)) {
                return issue;
            }
            if (!item_is_at(game.world_, args.item_id, ItemHolder::npc, npc_id)) {
                return "NPC does not hold that item";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::TakeItem &args) const {
            if (auto issue = present_check()) {
                return issue;
            }
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
            if (!contains(policy.allowed_locations, args.location_id)) {
                return "Location not allowed";
            }
            if (npc.state.current_location == args.location_id) {
                return "You are already there";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::RevealKnowledge &args) const {
            if (auto issue = present_check()) {
                return issue;
            }
            if (!contains(npc.identity.knowledge, args.fact_id)) {
                return "NPC does not know that fact";
            }
            if (!contains(policy.allowed_facts, args.fact_id)) {
                return "Fact not allowed";
            }
            if (!game.world_.facts.contains(args.fact_id)) {
                return "Unknown fact";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::Remember &args) const {
            if (single_line(args.summary).empty()) {
                return "Memory summary required";
            }
            if (args.summary.size() > tools::Remember::max_summary_bytes) {
                return "Memory summary must be at most " +
                       std::to_string(tools::Remember::max_summary_bytes) + " bytes";
            }
            if (args.importance < 1 || args.importance > 10) {
                return "Memory importance must be between 1 and 10";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::SetFlag &args) const {
            if (!game.world_.flags.contains(args.flag_id) &&
                !game.world_.flag_meta.contains(args.flag_id)) {
                return "Unknown flag";
            }
            if (!contains(policy.allowed_flags, args.flag_id)) {
                return "Flag not allowed";
            }
            return std::nullopt;
        }
        std::optional<std::string> operator()(const tools::InspectItem &args) const {
            return item_check(args.item_id);
        }
    };
    return std::visit(Visitor{*this, npc, npc_id, policy, check_item, check_present}, call);
}

std::optional<GameEvent>
CartridgeGame::narrate(const std::string &key,
                       const std::map<std::string, std::string> &args) const {
    const auto it = world_.config.action_narration_templates.find(key);
    if (it == world_.config.action_narration_templates.end() || it->second.empty()) {
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

        // The cartridge's narration of an accepted tool; an empty template is silence.
        [[nodiscard]] GameEvents narrated(const std::string &tool,
                                          std::map<std::string, std::string> args) const {
            args["npc"] = npc.identity.name;
            const auto event = game.narrate(tool, args);
            return event ? GameEvents{*event} : GameEvents{};
        }

        ToolOutcome operator()(const tools::Say &args) const {
            return GameEvents{{EventKind::dialogue, dialogue_line(npc.identity.name, args.text)}};
        }
        ToolOutcome operator()(const tools::GiveItem &args) const {
            const auto &item = game.world_.items.at(args.item_id);
            if (auto rejected = submit_action(actions::RelocateItem{
                    .item_id = args.item_id,
                    .destination = ItemPosition{.holder = ItemHolder::player, .id = {}},
                })) {
                return std::unexpected(std::move(*rejected));
            }
            return narrated("give_item", {{"item", item.name}});
        }
        ToolOutcome operator()(const tools::TakeItem &args) const {
            const auto &item = game.world_.items.at(args.item_id);
            if (auto rejected = submit_action(actions::RelocateItem{
                    .item_id = args.item_id,
                    .destination = ItemPosition{.holder = ItemHolder::npc, .id = npc_id},
                })) {
                return std::unexpected(std::move(*rejected));
            }
            return narrated("take_item", {{"item", item.name}});
        }
        ToolOutcome operator()(const tools::UpdateMood &args) const {
            const std::string mood = tools::to_string(args.mood);
            const bool unchanged = npc.state.mood == mood;
            if (auto rejected =
                    submit_action(actions::UpdateNpcMood{.npc_id = npc_id, .mood = mood})) {
                return std::unexpected(std::move(*rejected));
            }
            if (unchanged) {
                return GameEvents{};
            }
            return narrated("update_mood", {{"mood", mood}});
        }
        ToolOutcome operator()(const tools::UpdateTrust &args) const {
            if (auto rejected =
                    submit_action(actions::AdjustNpcTrust{.npc_id = npc_id, .delta = args.delta})) {
                return std::unexpected(std::move(*rejected));
            }
            return narrated("update_trust", {});
        }
        ToolOutcome operator()(const tools::MoveSelf &args) const {
            const std::string location_name = game.world_.locations.at(args.location_id).name;
            if (auto rejected = submit_action(actions::MoveNpc{
                    .npc_id = npc_id, .destination = args.location_id, .significant = true})) {
                return std::unexpected(std::move(*rejected));
            }
            return narrated("move_self", {{"location", location_name}});
        }
        ToolOutcome operator()(const tools::RevealKnowledge &args) const {
            const std::string fact_text = game.world_.facts.at(args.fact_id).text;
            const bool already_known = game.world_.revealed_facts.contains(args.fact_id);
            if (auto rejected = submit_action(actions::RevealFact{.fact_id = args.fact_id})) {
                return std::unexpected(std::move(*rejected));
            }
            if (already_known) {
                return GameEvents{};
            }
            GameEvents events = narrated("reveal_knowledge", {});
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
                            .summary = single_line(args.summary),
                            .importance = args.importance,
                            .related_npc = "",
                            .related_item = "",
                        },
                })) {
                return std::unexpected(std::move(*rejected));
            }
            return narrated("remember", {});
        }
        ToolOutcome operator()(const tools::SetFlag &args) const {
            if (auto rejected =
                    submit_action(actions::SetFlag{.flag_id = args.flag_id, .value = args.value})) {
                return std::unexpected(std::move(*rejected));
            }
            return narrated("set_flag", {});
        }
        ToolOutcome operator()(const tools::InspectItem &args) const {
            const auto &item = game.world_.items.at(args.item_id);
            return GameEvents{{EventKind::tool_result, item.name + ": " + item_text(item)}};
        }
    };
    return std::visit(Visitor{*this, npc, npc_id, submit}, call);
}

} // namespace chronicle
