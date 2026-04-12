#include "ai/tool_registry.hpp"
#include <zoo/agent.hpp>
#include <algorithm>
#include <sstream>

namespace chronicle {

ToolRegistry::ToolRegistry(const World &world) : world_(world) {}

// ---------------------------------------------------------------------------
// Validation helpers
// ---------------------------------------------------------------------------

bool ToolRegistry::npc_exists(const std::string &npc_id) const {
    return world_.npcs.contains(npc_id);
}

bool ToolRegistry::npc_has_item(const std::string &npc_id, const std::string &item_id) const {
    const auto &inv = world_.npcs.at(npc_id).state.inventory;
    return std::ranges::contains(inv, item_id);
}

bool ToolRegistry::player_has_item(const std::string &item_id) const {
    return world_.player.has_item(item_id);
}

bool ToolRegistry::is_key_item(const std::string &item_id) const {
    return world_.items.at(item_id).key_item;
}

bool ToolRegistry::item_exists(const std::string &item_id) const {
    return world_.items.contains(item_id);
}

bool ToolRegistry::location_exists(const std::string &location_id) const {
    return world_.locations.contains(location_id);
}

bool ToolRegistry::flag_exists(const std::string &flag_id) const {
    return world_.flags.contains(flag_id);
}

bool ToolRegistry::is_valid_mood(const std::string &mood) const {
    return kValidMoods.contains(mood);
}

// ---------------------------------------------------------------------------
// Helper: format an inventory as a bracketed list for error messages
// ---------------------------------------------------------------------------

static std::string format_inventory(const std::vector<std::string> &inv) {
    std::ostringstream oss;
    oss << "[";
    for (std::size_t i = 0; i < inv.size(); ++i) {
        if (i > 0)
            oss << ", ";
        oss << inv[i];
    }
    oss << "]";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Validation methods
// ---------------------------------------------------------------------------

ToolRegistry::ValidationResult ToolRegistry::validate_give_item(const std::string &npc_id,
                                                                const std::string &item_id) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (!item_exists(item_id))
        return "Error: Item '" + item_id + "' does not exist.";
    if (!npc_has_item(npc_id, item_id)) {
        const auto &inv = world_.npcs.at(npc_id).state.inventory;
        return "Error: NPC '" + npc_id + "' does not have item '" + item_id +
               "'. Inventory: " + format_inventory(inv);
    }
    return MutationRequest{
        MutationRequest::Type::GiveItemToPlayer, npc_id, {{"item_id", item_id}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_take_item(const std::string &npc_id,
                                                                const std::string &item_id) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (!item_exists(item_id))
        return "Error: Item '" + item_id + "' does not exist.";
    if (!player_has_item(item_id)) {
        const auto &inv = world_.player.inventory;
        return "Error: Player does not have item '" + item_id +
               "'. Inventory: " + format_inventory(inv);
    }
    if (is_key_item(item_id))
        return "Error: Item '" + item_id + "' is a key item and cannot be taken.";
    return MutationRequest{
        MutationRequest::Type::TakeItemFromPlayer, npc_id, {{"item_id", item_id}}};
}

ToolRegistry::ValidationResult
ToolRegistry::validate_update_mood(const std::string &npc_id, const std::string &mood) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (!is_valid_mood(mood))
        return "Error: '" + mood +
               "' is not a valid mood. Valid moods: neutral, suspicious, friendly, hostile, "
               "fearful, grieving.";
    return MutationRequest{MutationRequest::Type::UpdateNpcMood, npc_id, {{"mood", mood}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_update_trust(const std::string &npc_id,
                                                                   int delta) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";

    int current = world_.npcs.at(npc_id).state.trust_toward_player;
    int new_trust = std::clamp(current + delta, -100, 100);
    int clamped_delta = new_trust - current;

    return MutationRequest{MutationRequest::Type::UpdateNpcTrust,
                           npc_id,
                           {{"delta", std::to_string(clamped_delta)}}};
}

ToolRegistry::ValidationResult
ToolRegistry::validate_move_npc(const std::string &npc_id,
                                const std::string &location_id) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (!location_exists(location_id))
        return "Error: Location '" + location_id + "' does not exist.";
    return MutationRequest{
        MutationRequest::Type::MoveNpc, npc_id, {{"location_id", location_id}}};
}

ToolRegistry::ValidationResult
ToolRegistry::validate_reveal_knowledge(const std::string &npc_id,
                                        const std::string &fact_id) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";

    const auto &knowledge = world_.npcs.at(npc_id).identity.knowledge;
    if (!std::ranges::contains(knowledge, fact_id))
        return "Error: NPC '" + npc_id + "' does not know fact '" + fact_id + "'.";

    return MutationRequest{
        MutationRequest::Type::RevealKnowledge, npc_id, {{"fact_id", fact_id}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_add_memory(const std::string &npc_id,
                                                                 const std::string &summary,
                                                                 int importance) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (summary.empty())
        return "Error: Memory summary must not be empty.";

    int clamped = std::clamp(importance, 1, 10);
    return MutationRequest{MutationRequest::Type::AddMemory,
                           npc_id,
                           {{"summary", summary}, {"importance", std::to_string(clamped)}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_set_flag(const std::string &flag_id,
                                                               bool value) const {
    if (!flag_exists(flag_id))
        return "Error: Flag '" + flag_id + "' does not exist in world flags.";
    return MutationRequest{MutationRequest::Type::SetFlag,
                           "",
                           {{"flag_id", flag_id}, {"value", value ? "true" : "false"}}};
}

// ---------------------------------------------------------------------------
// Register methods: validate + enqueue
// ---------------------------------------------------------------------------

std::optional<std::string> ToolRegistry::register_give_item(const std::string &npc_id,
                                                             const std::string &item_id) {
    auto result = validate_give_item(npc_id, item_id);
    if (auto *error = std::get_if<std::string>(&result))
        return *error;
    pending_.push_back(std::get<MutationRequest>(result));
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::register_take_item(const std::string &npc_id,
                                                             const std::string &item_id) {
    auto result = validate_take_item(npc_id, item_id);
    if (auto *error = std::get_if<std::string>(&result))
        return *error;
    pending_.push_back(std::get<MutationRequest>(result));
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::register_update_mood(const std::string &npc_id,
                                                               const std::string &mood) {
    auto result = validate_update_mood(npc_id, mood);
    if (auto *error = std::get_if<std::string>(&result))
        return *error;
    pending_.push_back(std::get<MutationRequest>(result));
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::register_update_trust(const std::string &npc_id,
                                                                int delta) {
    auto result = validate_update_trust(npc_id, delta);
    if (auto *error = std::get_if<std::string>(&result))
        return *error;
    pending_.push_back(std::get<MutationRequest>(result));
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::register_move_npc(const std::string &npc_id,
                                                            const std::string &location_id) {
    auto result = validate_move_npc(npc_id, location_id);
    if (auto *error = std::get_if<std::string>(&result))
        return *error;
    pending_.push_back(std::get<MutationRequest>(result));
    return std::nullopt;
}

std::optional<std::string>
ToolRegistry::register_reveal_knowledge(const std::string &npc_id, const std::string &fact_id) {
    auto result = validate_reveal_knowledge(npc_id, fact_id);
    if (auto *error = std::get_if<std::string>(&result))
        return *error;
    pending_.push_back(std::get<MutationRequest>(result));
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::register_add_memory(const std::string &npc_id,
                                                              const std::string &summary,
                                                              int importance) {
    auto result = validate_add_memory(npc_id, summary, importance);
    if (auto *error = std::get_if<std::string>(&result))
        return *error;
    pending_.push_back(std::get<MutationRequest>(result));
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::register_set_flag(const std::string &flag_id,
                                                            bool value) {
    auto result = validate_set_flag(flag_id, value);
    if (auto *error = std::get_if<std::string>(&result))
        return *error;
    pending_.push_back(std::get<MutationRequest>(result));
    return std::nullopt;
}

void ToolRegistry::register_say(const std::string &npc_id, const std::string &dialogue) {
    dialogue_log_.emplace_back(npc_id, dialogue);
}

const std::vector<MutationRequest> &ToolRegistry::pending_mutations() const {
    return pending_;
}

void ToolRegistry::clear_pending() {
    pending_.clear();
}

void ToolRegistry::register_tools(zoo::Agent& agent, const std::string& npc_id) {
    auto say_func = [this, npc_id](std::string dialogue) -> std::string {
        this->register_say(npc_id, dialogue);
        return "OK";
    };
    (void)agent.register_tool("say", "Speak a dialogue line", {"dialogue"}, std::move(say_func));

    auto give_item_func = [this, npc_id](std::string item_id) -> std::string {
        if (auto err = this->register_give_item(npc_id, item_id)) return *err;
        return "OK";
    };
    (void)agent.register_tool("give_item", "Give an item to the player", {"item_id"}, std::move(give_item_func));

    auto take_item_func = [this, npc_id](std::string item_id) -> std::string {
        if (auto err = this->register_take_item(npc_id, item_id)) return *err;
        return "OK";
    };
    (void)agent.register_tool("take_item", "Take an item from the player", {"item_id"}, std::move(take_item_func));

    auto update_mood_func = [this, npc_id](std::string mood) -> std::string {
        if (auto err = this->register_update_mood(npc_id, mood)) return *err;
        return "OK";
    };
    (void)agent.register_tool("update_mood", "Change the NPC's current mood", {"mood"}, std::move(update_mood_func));

    auto update_trust_func = [this, npc_id](std::string delta_str) -> std::string {
        try {
            int delta = std::stoi(delta_str);
            if (auto err = this->register_update_trust(npc_id, delta)) return *err;
            return "OK";
        } catch (const std::exception&) {
            return "Error: trust delta must be a valid integer.";
        }
    };
    (void)agent.register_tool("update_trust", "Change the player's trust level with this NPC (-100 to 100)", {"delta"}, std::move(update_trust_func));

    auto move_self_func = [this, npc_id](std::string location_id) -> std::string {
        if (auto err = this->register_move_npc(npc_id, location_id)) return *err;
        return "OK";
    };
    (void)agent.register_tool("move_self", "Move the NPC to a new location", {"location_id"}, std::move(move_self_func));

    auto reveal_knowledge_func = [this, npc_id](std::string fact_id) -> std::string {
        if (auto err = this->register_reveal_knowledge(npc_id, fact_id)) return *err;
        return "OK";
    };
    (void)agent.register_tool("reveal_knowledge", "Reveal a known fact to the player", {"fact_id"}, std::move(reveal_knowledge_func));

    auto remember_func = [this, npc_id](std::string summary, std::string importance_str) -> std::string {
        try {
            int importance = std::stoi(importance_str);
            if (auto err = this->register_add_memory(npc_id, summary, importance)) return *err;
            return "OK";
        } catch (const std::exception&) {
            return "Error: importance must be a valid integer.";
        }
    };
    (void)agent.register_tool("remember", "Save a memory of an event or interaction", {"summary", "importance"}, std::move(remember_func));

    auto set_flag_func = [this](std::string flag_id, std::string value_str) -> std::string {
        bool value = (value_str == "true" || value_str == "1");
        if (auto err = this->register_set_flag(flag_id, value)) return *err;
        return "OK";
    };
    (void)agent.register_tool("set_flag", "Set or update a world narrative flag", {"flag_id", "value"}, std::move(set_flag_func));
}

} // namespace chronicle
