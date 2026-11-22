#include "chronicle/prompt.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>

#include "chronicle/game/npc_tools.hpp"
#include "chronicle/game/text_utils.hpp"

namespace chronicle {

namespace {

using game_detail::contains;
using game_detail::single_line;

constexpr std::string_view PLAYER_SECTION = "[Player]\n";

std::size_t utf8_prefix(const std::string &text, std::size_t bytes) {
    bytes = std::min(bytes, text.size());
    while (bytes > 0 && bytes < text.size() &&
           (static_cast<unsigned char>(text[bytes]) & 0xC0U) == 0x80U) {
        --bytes;
    }
    return bytes;
}

std::string bounded_prompt_text(const std::string &text, const int token_budget) {
    const auto byte_budget = static_cast<std::size_t>(std::max(1, token_budget)) * 4U;
    if (text.size() <= byte_budget) {
        return text;
    }
    constexpr std::string_view marker = "\n[truncated]";
    if (byte_budget <= marker.size()) {
        return text.substr(0, utf8_prefix(text, byte_budget));
    }
    const auto prefix = utf8_prefix(text, byte_budget - marker.size());
    return text.substr(0, prefix) + std::string(marker);
}

bool may_use(const ToolPolicy &policy, const std::string_view tool) {
    return contains(policy.allowed_tools, std::string(tool));
}

// An item by name, with the id its tools take when the NPC may handle it.
std::string item_label(const WorldState &world, const ToolPolicy &policy,
                       const std::string &item_id) {
    const std::string &name = world.items.at(item_id).name;
    return contains(policy.allowed_items, item_id) ? name + " [" + item_id + "]" : name;
}

void write_list(std::ostream &out, const std::string_view label,
                const std::vector<std::string> &values, const std::string_view when_empty) {
    out << label << ": ";
    if (values.empty()) {
        out << when_empty;
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
        out << (i == 0 ? "" : ", ") << values[i];
    }
    out << "\n";
}

void write_facts(std::ostream &out, const WorldState &world, const std::string_view label,
                 const std::vector<std::string> &fact_ids) {
    out << label << ":\n";
    for (const auto &fact_id : fact_ids) {
        out << "- [" << fact_id << "] " << world.facts.at(fact_id).text << "\n";
    }
    if (fact_ids.empty()) {
        out << "- (none)\n";
    }
}

// What the NPC knows, split by whether its tools may put a fact on record.
void write_knowledge(std::ostream &out, const WorldState &world, const NpcIdentity &identity) {
    const auto &policy = identity.tool_policy;
    if (!may_use(policy, tools::RevealKnowledge::name)) {
        write_facts(out, world, "Knowledge", identity.knowledge);
        return;
    }
    std::vector<std::string> revealable;
    std::vector<std::string> kept;
    for (const auto &fact_id : identity.knowledge) {
        (contains(policy.allowed_facts, fact_id) ? revealable : kept).push_back(fact_id);
    }
    write_facts(out, world, "Knowledge you may reveal with reveal_knowledge", revealable);
    if (!kept.empty()) {
        write_facts(out, world, "Knowledge you keep to yourself", kept);
    }
}

// The ids a tool accepts. A model has no other way to learn them.
void write_tool_scopes(std::ostream &out, const WorldState &world, const ToolPolicy &policy) {
    if ((may_use(policy, tools::GiveItem::name) || may_use(policy, tools::TakeItem::name) ||
         may_use(policy, tools::InspectItem::name)) &&
        !policy.allowed_items.empty()) {
        out << "Items you may handle:\n";
        for (const auto &item_id : policy.allowed_items) {
            out << "- [" << item_id << "] " << world.items.at(item_id).name << "\n";
        }
    }
    if (may_use(policy, tools::MoveSelf::name) && !policy.allowed_locations.empty()) {
        out << "Places you may go with move_self:\n";
        for (const auto &location_id : policy.allowed_locations) {
            out << "- [" << location_id << "] " << world.locations.at(location_id).name << "\n";
        }
    }
    if (may_use(policy, tools::SetFlag::name) && !policy.allowed_flags.empty()) {
        out << "Flags you may set with set_flag:\n";
        for (const auto &flag_id : policy.allowed_flags) {
            out << "- [" << flag_id << "] " << world.flag_meta.at(flag_id).description << "\n";
        }
    }
}

// The most important memories that fit, and the newest of equally important ones.
std::string memories_within(const std::vector<MemoryEntry> &memories, const int token_budget) {
    std::vector<const MemoryEntry *> ranked;
    for (auto it = memories.rbegin(); it != memories.rend(); ++it) {
        ranked.push_back(&*it);
    }
    std::ranges::stable_sort(ranked, std::ranges::greater{}, &MemoryEntry::importance);

    std::string text;
    auto budget = static_cast<std::size_t>(std::max(1, token_budget)) * 4U;
    for (const auto *memory : ranked) {
        const std::string line =
            "- (" + std::to_string(memory->importance) + ") " + single_line(memory->summary) + "\n";
        if (line.size() > budget) {
            continue;
        }
        budget -= line.size();
        text += line;
    }
    return text.empty() ? "- (none)\n" : text;
}

} // namespace

std::string build_npc_system_prompt(const WorldState &world, const std::string &npc_id) {
    const auto &identity = world.npcs.at(npc_id).identity;
    const auto &policy = identity.tool_policy;

    std::ostringstream out;
    out << "You are " << identity.name << ", " << identity.role << ".\n";
    out << "Personality: " << identity.personality_summary << "\n";
    out << "Backstory: " << identity.backstory << "\n";
    out << "Goals:\n";
    for (const auto &goal : identity.goals) {
        out << "- " << goal << "\n";
    }
    write_knowledge(out, world, identity);
    write_tool_scopes(out, world, policy);
    out << "Rules:\n"
        << "- Stay in character.\n"
        << "- Each message holds your private [Context] and what the player said.\n"
        << "- Use tools to act; do not invent facts that are not in your knowledge list.\n"
        << "- A tool takes the id shown in [brackets], written exactly.\n"
        << "- Check each tool result's ok field; when false, use error to choose another action.\n"
        << "- Only reveal secrets when trust is high enough and the secret is authored.\n";
    if (may_use(policy, tools::MoveSelf::name)) {
        out << "- Speak before you move away; the player cannot hear you afterwards.\n";
    }
    if (may_use(policy, tools::Say::name)) {
        out << "- Speak with the say tool. Say everything in one call, then end your turn.";
    } else {
        out << "- Answer the player in plain text.";
    }
    return out.str();
}

std::string build_npc_turn_message(const WorldState &world, const std::string &npc_id,
                                   const std::string &player_text) {
    const auto &npc = world.npcs.at(npc_id);
    const auto &state = npc.state;
    const auto &identity = npc.identity;
    const auto &policy = identity.tool_policy;
    const auto &loc = world.locations.at(state.current_location);

    const auto labels = [&](const std::vector<std::string> &item_ids) {
        std::vector<std::string> result;
        for (const auto &item_id : item_ids) {
            if (world.items.contains(item_id)) {
                result.push_back(item_label(world, policy, item_id));
            }
        }
        return result;
    };

    // Most useful first: a budget too small for all of this cuts from the end.
    std::ostringstream context;
    context << "Time: " << world.clock.period_name() << " (turn " << world.clock.turns_elapsed
            << ")\n";
    context << "Your location: " << loc.name << "\n";
    context << "Mood: " << state.mood << "\n";
    context << "Trust toward player: " << state.trust_toward_player << "\n";
    if (!identity.secret.empty() && state.trust_toward_player >= identity.trust_reveal_threshold) {
        context << "Secret you may reveal carefully: " << identity.secret << "\n";
    }
    write_list(context, "You hold", labels(items_at(world, ItemHolder::npc, npc_id)), "nothing");
    write_list(context, "Player carries", labels(items_at(world, ItemHolder::player)), "nothing");

    std::vector<std::string> known;
    for (const auto &fact_id : identity.knowledge) {
        if (world.revealed_facts.contains(fact_id)) {
            known.push_back("[" + fact_id + "]");
        }
    }
    if (!known.empty()) {
        write_list(context, "The player already knows", known, "");
    }
    if (may_use(policy, tools::SetFlag::name) && !policy.allowed_flags.empty()) {
        std::vector<std::string> flags;
        for (const auto &flag_id : policy.allowed_flags) {
            flags.push_back("[" + flag_id + (world.flags.at(flag_id) ? "] is true" : "] is false"));
        }
        write_list(context, "Flags", flags, "");
    }

    std::vector<std::string> company;
    for (const auto &[nid, other] : world.npcs) {
        if (nid != npc_id && other.state.current_location == state.current_location) {
            company.push_back(other.identity.name);
        }
    }
    write_list(context, "Also here", company, "no one");

    auto lying_here = items_at(world, ItemHolder::location, state.current_location);
    std::erase_if(lying_here, [&](const std::string &item_id) {
        const auto item = world.items.find(item_id);
        return item == world.items.end() || item->second.hidden;
    });
    write_list(context, "Visible items", labels(lying_here), "none");
    context << "Location description: " << loc.base_description << "\n";

    std::ostringstream out;
    out << "[Context]\n"
        << bounded_prompt_text(context.str(), world.config.max_world_tokens) << "\nMemories:\n"
        << memories_within(state.memories, world.config.max_memory_tokens) << "\n"
        << PLAYER_SECTION;
    const nlohmann::json payload{
        {"player_said", bounded_prompt_text(player_text, world.config.max_history_tokens)}};
    out << payload.dump();
    return out.str();
}

std::string without_turn_context(const std::string &turn_message) {
    if (!turn_message.starts_with("[Context]\n")) {
        return turn_message;
    }
    // The player's words are one line of JSON, so the last marker is the real one.
    const auto player = turn_message.rfind("\n" + std::string(PLAYER_SECTION));
    return player == std::string::npos ? turn_message : turn_message.substr(player + 1);
}

} // namespace chronicle
