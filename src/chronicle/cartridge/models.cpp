#include "chronicle/cartridge/models.hpp"

#include <algorithm>
#include <stdexcept>

namespace chronicle {

using nlohmann::json;

namespace {

// Read an optional field, falling back to the member's current (default) value.
template <typename T> void opt(const json &j, const char *key, T &out) {
    const auto it = j.find(key);
    if (it != j.end() && !it->is_null()) {
        it->get_to(out);
    }
}

template <typename T> void req(const json &j, const char *key, T &out) {
    j.at(key).get_to(out);
}

} // namespace

const std::vector<std::string> &valid_moods() {
    static const std::vector<std::string> moods = {"fearful", "friendly", "grieving",
                                                   "hostile", "neutral",  "suspicious"};
    return moods;
}

const std::vector<std::string> &npc_tool_names() {
    static const std::vector<std::string> tools = {
        "say",       "give_item",        "take_item", "update_mood", "update_trust",
        "move_self", "reveal_knowledge", "remember",  "set_flag",    "inspect_item"};
    return tools;
}

const std::vector<std::string> &period_names() {
    static const std::vector<std::string> periods = {"morning", "afternoon", "evening", "night"};
    return periods;
}

bool ConfigData::has_llm_endpoint() const {
    const auto has_text = [](const std::string &s) {
        return s.find_first_not_of(" \t\r\n") != std::string::npos;
    };
    return has_text(llm_base_url) && has_text(llm_model);
}

std::map<std::string, std::string> default_narration_templates() {
    return {
        {"give_item_to_player", "{npc} hands you the {item}."},
        {"take_item_from_player", "{npc} takes the {item}."},
        {"update_npc_mood", "{npc}'s expression shifts - they seem {mood} now."},
        {"move_npc", "{npc} excuses themselves and leaves."},
        {"reveal_knowledge", ""},
        {"update_npc_trust", ""},
        {"add_memory", ""},
        {"set_flag", ""},
    };
}

std::set<std::string> LocationData::locked_directions() const {
    std::set<std::string> locked;
    for (const auto &entry : locked_exits) {
        if (!entry.direction.empty() && !entry.unlocked) {
            locked.insert(entry.direction);
        }
    }
    return locked;
}

int ClockState::period_index() const {
    if (turns_per_period <= 0) {
        return 0;
    }
    return turns_elapsed / turns_per_period;
}

std::string ClockState::period_name() const {
    const auto &names = period_names();
    return names[static_cast<std::size_t>(period_index()) % names.size()];
}

bool ClockState::time_expired() const {
    return period_index() >= total_periods;
}

// --- from_json -----------------------------------------------------------

void from_json(const json &j, ScenarioFiles &v) {
    req(j, "config", v.config);
    req(j, "world", v.world);
    req(j, "npcs", v.npcs);
    req(j, "facts", v.facts);
    req(j, "flags", v.flags);
    req(j, "events", v.events);
}

void from_json(const json &j, ScenarioMetadata &v) {
    opt(j, "description", v.description);
    opt(j, "author", v.author);
    opt(j, "license", v.license);
}

void from_json(const json &j, ScenarioManifest &v) {
    req(j, "id", v.id);
    req(j, "name", v.name);
    req(j, "version", v.version);
    req(j, "chronicle_schema_version", v.chronicle_schema_version);
    req(j, "files", v.files);
    opt(j, "metadata", v.metadata);
}

void from_json(const json &j, ConfigData &v) {
    opt(j, "temperature", v.temperature);
    opt(j, "max_response_tokens", v.max_response_tokens);
    opt(j, "inference_timeout_ms", v.inference_timeout_ms);
    opt(j, "turns_per_period", v.turns_per_period);
    opt(j, "total_periods", v.total_periods);
    opt(j, "max_memory_tokens", v.max_memory_tokens);
    opt(j, "max_world_tokens", v.max_world_tokens);
    opt(j, "max_history_tokens", v.max_history_tokens);
    opt(j, "save_directory", v.save_directory);
    opt(j, "use_tui", v.use_tui);
    opt(j, "use_color", v.use_color);
    opt(j, "llm_base_url", v.llm_base_url);
    opt(j, "llm_model", v.llm_model);
    opt(j, "llm_api_key", v.llm_api_key);
    opt(j, "verb_aliases", v.verb_aliases);
    v.mutation_narration_templates = default_narration_templates();
    opt(j, "mutation_narration_templates", v.mutation_narration_templates);
}

void from_json(const json &j, LockedExitEntry &v) {
    if (j.is_string()) {
        v.direction = j.get<std::string>();
        v.unlocked = false;
        return;
    }
    opt(j, "direction", v.direction);
    opt(j, "unlocked", v.unlocked);
}

void from_json(const json &j, LocationData &v) {
    req(j, "name", v.name);
    req(j, "base_description", v.base_description);
    opt(j, "exits", v.exits);
    opt(j, "items", v.items);
    opt(j, "npcs", v.npcs);
    opt(j, "locked_exits", v.locked_exits);
}

void from_json(const json &j, ItemData &v) {
    req(j, "name", v.name);
    req(j, "description", v.description);
    opt(j, "takeable", v.takeable);
    opt(j, "key_item", v.key_item);
    opt(j, "hidden", v.hidden);
    opt(j, "unlock_target", v.unlock_target);
    opt(j, "properties", v.properties);
}

void from_json(const json &j, WorldData &v) {
    req(j, "start_location", v.start_location);
    req(j, "locations", v.locations);
    opt(j, "items", v.items);
}

void from_json(const json &j, ToolPolicy &v) {
    opt(j, "allowed_tools", v.allowed_tools);
    opt(j, "allowed_items", v.allowed_items);
    opt(j, "allowed_facts", v.allowed_facts);
    opt(j, "allowed_flags", v.allowed_flags);
    opt(j, "allowed_locations", v.allowed_locations);
}

void from_json(const json &j, NpcIdentity &v) {
    opt(j, "id", v.id);
    req(j, "name", v.name);
    opt(j, "role", v.role);
    opt(j, "personality_summary", v.personality_summary);
    opt(j, "backstory", v.backstory);
    opt(j, "secret", v.secret);
    opt(j, "goals", v.goals);
    opt(j, "knowledge", v.knowledge);
    opt(j, "trust_reveal_threshold", v.trust_reveal_threshold);
    opt(j, "tool_policy", v.tool_policy);
}

void from_json(const json &j, MemoryEntry &v) {
    opt(j, "timestamp", v.timestamp);
    opt(j, "type", v.type);
    req(j, "summary", v.summary);
    opt(j, "importance", v.importance);
    opt(j, "related_npc", v.related_npc);
    opt(j, "related_item", v.related_item);
}

void from_json(const json &j, NpcState &v) {
    req(j, "current_location", v.current_location);
    opt(j, "mood", v.mood);
    if (std::ranges::find(valid_moods(), v.mood) == valid_moods().end()) {
        throw std::invalid_argument("invalid mood: " + v.mood);
    }
    opt(j, "trust_toward_player", v.trust_toward_player);
    opt(j, "inventory", v.inventory);
    opt(j, "memories", v.memories);
    opt(j, "has_met_player", v.has_met_player);
    opt(j, "secret_revealed", v.secret_revealed);
}

void from_json(const json &j, NpcData &v) {
    req(j, "identity", v.identity);
    req(j, "state", v.state);
}

void from_json(const json &j, NpcsFile &v) {
    req(j, "npcs", v.npcs);
}

void from_json(const json &j, FactData &v) {
    req(j, "text", v.text);
    opt(j, "category", v.category);
    opt(j, "revealed_by_default", v.revealed_by_default);
}

void from_json(const json &j, FactsFile &v) {
    opt(j, "facts", v.facts);
}

void from_json(const json &j, FlagData &v) {
    opt(j, "default", v.default_value);
    opt(j, "description", v.description);
}

void from_json(const json &j, FlagsFile &v) {
    opt(j, "flags", v.flags);
}

void from_json(const json &j, ConditionData &v) {
    req(j, "type", v.type);
    opt(j, "args", v.args);
}

void from_json(const json &j, EventActionData &v) {
    req(j, "type", v.type);
    const auto it = j.find("params");
    if (it != j.end() && it->is_object()) {
        v.params = *it;
    }
}

void from_json(const json &j, EventTriggerData &v) {
    opt(j, "conditions", v.conditions);
    opt(j, "actions", v.actions);
    opt(j, "once", v.once);
    opt(j, "fired", v.fired);
}

void from_json(const json &j, EventsFile &v) {
    opt(j, "events", v.events);
}

void from_json(const json &j, PlayerState &v) {
    req(j, "current_location", v.current_location);
    opt(j, "inventory", v.inventory);
}

void from_json(const json &j, ClockState &v) {
    opt(j, "turns_elapsed", v.turns_elapsed);
    opt(j, "turns_per_period", v.turns_per_period);
    opt(j, "total_periods", v.total_periods);
}

void from_json(const json &j, WorldState &v) {
    req(j, "manifest", v.manifest);
    req(j, "config", v.config);
    req(j, "locations", v.locations);
    opt(j, "items", v.items);
    opt(j, "npcs", v.npcs);
    opt(j, "facts", v.facts);
    opt(j, "flags", v.flags);
    opt(j, "flag_meta", v.flag_meta);
    opt(j, "events", v.events);
    req(j, "player", v.player);
    req(j, "clock", v.clock);
    opt(j, "revealed_facts", v.revealed_facts);
    opt(j, "item_locations", v.item_locations);
    opt(j, "item_owners", v.item_owners);
}

// --- to_json -------------------------------------------------------------

void to_json(json &j, const ScenarioFiles &v) {
    j = json{{"config", v.config}, {"world", v.world}, {"npcs", v.npcs},
             {"facts", v.facts},   {"flags", v.flags}, {"events", v.events}};
}

void to_json(json &j, const ScenarioMetadata &v) {
    j = json{{"description", v.description}, {"author", v.author}, {"license", v.license}};
}

void to_json(json &j, const ScenarioManifest &v) {
    j = json{{"id", v.id},           {"name", v.name},
             {"version", v.version}, {"chronicle_schema_version", v.chronicle_schema_version},
             {"files", v.files},     {"metadata", v.metadata}};
}

void to_json(json &j, const ConfigData &v) {
    j = json{{"temperature", v.temperature},
             {"max_response_tokens", v.max_response_tokens},
             {"inference_timeout_ms", v.inference_timeout_ms},
             {"turns_per_period", v.turns_per_period},
             {"total_periods", v.total_periods},
             {"max_memory_tokens", v.max_memory_tokens},
             {"max_world_tokens", v.max_world_tokens},
             {"max_history_tokens", v.max_history_tokens},
             {"save_directory", v.save_directory},
             {"use_tui", v.use_tui},
             {"use_color", v.use_color},
             {"llm_base_url", v.llm_base_url},
             {"llm_model", v.llm_model},
             {"llm_api_key", v.llm_api_key},
             {"verb_aliases", v.verb_aliases},
             {"mutation_narration_templates", v.mutation_narration_templates}};
}

void to_json(json &j, const LockedExitEntry &v) {
    j = json{{"direction", v.direction}, {"unlocked", v.unlocked}};
}

void to_json(json &j, const LocationData &v) {
    j = json{{"name", v.name},   {"base_description", v.base_description},
             {"exits", v.exits}, {"items", v.items},
             {"npcs", v.npcs},   {"locked_exits", v.locked_exits}};
}

void to_json(json &j, const ItemData &v) {
    j = json{{"name", v.name},
             {"description", v.description},
             {"takeable", v.takeable},
             {"key_item", v.key_item},
             {"hidden", v.hidden},
             {"unlock_target", v.unlock_target},
             {"properties", v.properties}};
}

void to_json(json &j, const ToolPolicy &v) {
    j = json{{"allowed_tools", v.allowed_tools},
             {"allowed_items", v.allowed_items},
             {"allowed_facts", v.allowed_facts},
             {"allowed_flags", v.allowed_flags},
             {"allowed_locations", v.allowed_locations}};
}

void to_json(json &j, const NpcIdentity &v) {
    j = json{{"id", v.id},
             {"name", v.name},
             {"role", v.role},
             {"personality_summary", v.personality_summary},
             {"backstory", v.backstory},
             {"secret", v.secret},
             {"goals", v.goals},
             {"knowledge", v.knowledge},
             {"trust_reveal_threshold", v.trust_reveal_threshold},
             {"tool_policy", v.tool_policy}};
}

void to_json(json &j, const MemoryEntry &v) {
    j = json{{"timestamp", v.timestamp},     {"type", v.type},
             {"summary", v.summary},         {"importance", v.importance},
             {"related_npc", v.related_npc}, {"related_item", v.related_item}};
}

void to_json(json &j, const NpcState &v) {
    j = json{{"current_location", v.current_location},
             {"mood", v.mood},
             {"trust_toward_player", v.trust_toward_player},
             {"inventory", v.inventory},
             {"memories", v.memories},
             {"has_met_player", v.has_met_player},
             {"secret_revealed", v.secret_revealed}};
}

void to_json(json &j, const NpcData &v) {
    j = json{{"identity", v.identity}, {"state", v.state}};
}

void to_json(json &j, const FactData &v) {
    j = json{
        {"text", v.text}, {"category", v.category}, {"revealed_by_default", v.revealed_by_default}};
}

void to_json(json &j, const FlagData &v) {
    j = json{{"default", v.default_value}, {"description", v.description}};
}

void to_json(json &j, const ConditionData &v) {
    j = json{{"type", v.type}, {"args", v.args}};
}

void to_json(json &j, const EventActionData &v) {
    j = json{{"type", v.type}, {"params", v.params}};
}

void to_json(json &j, const EventTriggerData &v) {
    j = json{
        {"conditions", v.conditions}, {"actions", v.actions}, {"once", v.once}, {"fired", v.fired}};
}

void to_json(json &j, const PlayerState &v) {
    j = json{{"current_location", v.current_location}, {"inventory", v.inventory}};
}

void to_json(json &j, const ClockState &v) {
    j = json{{"turns_elapsed", v.turns_elapsed},
             {"turns_per_period", v.turns_per_period},
             {"total_periods", v.total_periods}};
}

void to_json(json &j, const WorldState &v) {
    j = json{{"manifest", v.manifest},
             {"config", v.config},
             {"locations", v.locations},
             {"items", v.items},
             {"npcs", v.npcs},
             {"facts", v.facts},
             {"flags", v.flags},
             {"flag_meta", v.flag_meta},
             {"events", v.events},
             {"player", v.player},
             {"clock", v.clock},
             {"revealed_facts", v.revealed_facts},
             {"item_locations", v.item_locations},
             {"item_owners", v.item_owners}};
}

} // namespace chronicle
