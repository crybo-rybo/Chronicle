// Data models for Chronicle cartridge packages (JSON in, JSON out).
#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

namespace chronicle {

inline constexpr int SCHEMA_VERSION = 1;

[[nodiscard]] const std::vector<std::string> &valid_moods();
[[nodiscard]] const std::vector<std::string> &period_names();

struct ScenarioFiles {
    std::string config;
    std::string world;
    std::string npcs;
    std::string facts;
    std::string flags;
    std::string events;
};

struct ScenarioMetadata {
    std::string description;
    std::string author;
    std::string license;
};

struct ScenarioManifest {
    std::string id;
    std::string name;
    std::string version;
    int chronicle_schema_version = 0;
    ScenarioFiles files;
    ScenarioMetadata metadata;
};

struct ConfigData {
    double temperature = 0.7;
    int max_response_tokens = 512;
    int inference_timeout_ms = 120'000;
    int turns_per_period = 5;
    int total_periods = 12;
    int max_memory_tokens = 800;
    int max_world_tokens = 400;
    int max_history_tokens = 600;
    std::map<std::string, std::string> verb_aliases;
    /// Player-facing text after an accepted NPC world write. Keys match public
    /// tool names (`give_item`, `remember`, …). Empty string suppresses
    /// narration. Legacy `mutation_narration_templates` / old keys still load.
    std::map<std::string, std::string> action_narration_templates;
};

[[nodiscard]] std::map<std::string, std::string> default_narration_templates();

// Cartridges may write a locked exit as a bare direction string or as an
// object {"direction": ..., "unlocked": ...}; both parse into this entry.
struct LockedExitEntry {
    std::string direction;
    bool unlocked = false;
};

struct LocationData {
    std::string name;
    std::string base_description;
    std::map<std::string, std::string> exits; // direction -> location_id
    std::vector<LockedExitEntry> locked_exits;

    [[nodiscard]] std::set<std::string> locked_directions() const;
};

// Cartridge-input shape. Initial item placement is consumed while assembling
// the runtime and does not remain as a second mutable source of truth.
struct AuthoredLocationData {
    LocationData location;
    std::vector<std::string> items;
};

struct ItemData {
    std::string name;
    std::string description;
    bool takeable = true;
    bool key_item = false;
    bool hidden = false;
    std::string unlock_target;
    std::map<std::string, std::string> properties;
};

struct WorldData {
    std::string start_location;
    std::map<std::string, AuthoredLocationData> locations;
    std::map<std::string, ItemData> items;
};

struct ToolPolicy {
    std::vector<std::string> allowed_tools;
    std::vector<std::string> allowed_items;
    std::vector<std::string> allowed_facts;
    std::vector<std::string> allowed_flags;
    std::vector<std::string> allowed_locations;
};

struct NpcIdentity {
    std::string id;
    std::string name;
    std::string role;
    std::string personality_summary;
    std::string backstory;
    std::string secret;
    std::vector<std::string> goals;
    std::vector<std::string> knowledge;
    int trust_reveal_threshold = 70;
    ToolPolicy tool_policy;
};

struct MemoryEntry {
    std::string timestamp;
    std::string type = "observation";
    std::string summary;
    int importance = 5;
    std::string related_npc;
    std::string related_item;
};

struct NpcState {
    std::string current_location;
    std::string mood = "neutral";
    int trust_toward_player = 0;
    std::vector<MemoryEntry> memories;
    bool has_met_player = false;
    bool secret_revealed = false;
};

struct AuthoredNpcState {
    NpcState state;
    std::vector<std::string> inventory;
};

struct NpcData {
    NpcIdentity identity;
    NpcState state;
};

struct AuthoredNpcData {
    NpcIdentity identity;
    AuthoredNpcState state;
};

struct NpcsFile {
    std::map<std::string, AuthoredNpcData> npcs;
};

struct FactData {
    std::string text;
    std::string category = "clue";
    bool revealed_by_default = false;
};

struct FactsFile {
    std::map<std::string, FactData> facts;
};

struct FlagData {
    bool default_value = false; // JSON key "default"
    std::string description;
};

struct FlagsFile {
    std::map<std::string, FlagData> flags;
};

struct ConditionData {
    std::string type;
    std::vector<std::string> args;
};

struct EventActionData {
    std::string type;
    nlohmann::json params = nlohmann::json::object();
};

struct EventTriggerData {
    std::vector<ConditionData> conditions;
    std::vector<EventActionData> actions;
    bool once = true;
    bool fired = false;
};

struct EventsFile {
    std::map<std::string, EventTriggerData> events;
};

struct PlayerState {
    std::string current_location;
};

enum class ItemHolder { location, player, npc };

struct ItemPosition {
    ItemHolder holder = ItemHolder::location;
    // Location or NPC id. Empty for the player.
    std::string id;

    [[nodiscard]] bool is_location(const std::string &location_id) const;
    [[nodiscard]] bool is_player() const;
    [[nodiscard]] bool is_npc(const std::string &npc_id) const;
    bool operator==(const ItemPosition &) const = default;
};

struct ClockState {
    int turns_elapsed = 0;
    int turns_per_period = 5;
    int total_periods = 12;

    [[nodiscard]] int period_index() const;
    [[nodiscard]] std::string period_name() const;
    [[nodiscard]] bool time_expired() const;
};

// Runtime mutable world assembled from a cartridge.
struct WorldState {
    ScenarioManifest manifest;
    ConfigData config;
    std::map<std::string, LocationData> locations;
    std::map<std::string, ItemData> items;
    std::map<std::string, NpcData> npcs;
    std::map<std::string, FactData> facts;
    std::map<std::string, bool> flags;
    std::map<std::string, FlagData> flag_meta;
    std::map<std::string, EventTriggerData> events;
    PlayerState player;
    ClockState clock;
    std::set<std::string> revealed_facts;
    // The sole runtime source of truth for item placement.
    std::map<std::string, ItemPosition> item_positions;
};

[[nodiscard]] std::vector<std::string> items_at(const WorldState &world, ItemHolder holder,
                                                const std::string &id = {});
[[nodiscard]] bool item_is_at(const WorldState &world, const std::string &item_id,
                              ItemHolder holder, const std::string &id = {});

// nlohmann ADL hooks. from_json throws nlohmann::json::exception on missing
// required fields or invalid values (e.g. an unknown mood).
void from_json(const nlohmann::json &j, ScenarioFiles &v);
void from_json(const nlohmann::json &j, ScenarioMetadata &v);
void from_json(const nlohmann::json &j, ScenarioManifest &v);
void from_json(const nlohmann::json &j, ConfigData &v);
void from_json(const nlohmann::json &j, LockedExitEntry &v);
void from_json(const nlohmann::json &j, LocationData &v);
void from_json(const nlohmann::json &j, AuthoredLocationData &v);
void from_json(const nlohmann::json &j, ItemData &v);
void from_json(const nlohmann::json &j, WorldData &v);
void from_json(const nlohmann::json &j, ToolPolicy &v);
void from_json(const nlohmann::json &j, NpcIdentity &v);
void from_json(const nlohmann::json &j, MemoryEntry &v);
void from_json(const nlohmann::json &j, NpcState &v);
void from_json(const nlohmann::json &j, AuthoredNpcState &v);
void from_json(const nlohmann::json &j, NpcData &v);
void from_json(const nlohmann::json &j, AuthoredNpcData &v);
void from_json(const nlohmann::json &j, NpcsFile &v);
void from_json(const nlohmann::json &j, FactData &v);
void from_json(const nlohmann::json &j, FactsFile &v);
void from_json(const nlohmann::json &j, FlagData &v);
void from_json(const nlohmann::json &j, FlagsFile &v);
void from_json(const nlohmann::json &j, ConditionData &v);
void from_json(const nlohmann::json &j, EventActionData &v);
void from_json(const nlohmann::json &j, EventTriggerData &v);
void from_json(const nlohmann::json &j, EventsFile &v);
void from_json(const nlohmann::json &j, PlayerState &v);
void from_json(const nlohmann::json &j, ItemPosition &v);
void from_json(const nlohmann::json &j, ClockState &v);
void from_json(const nlohmann::json &j, WorldState &v);

void to_json(nlohmann::json &j, const ScenarioFiles &v);
void to_json(nlohmann::json &j, const ScenarioMetadata &v);
void to_json(nlohmann::json &j, const ScenarioManifest &v);
void to_json(nlohmann::json &j, const ConfigData &v);
void to_json(nlohmann::json &j, const LockedExitEntry &v);
void to_json(nlohmann::json &j, const LocationData &v);
void to_json(nlohmann::json &j, const ItemData &v);
void to_json(nlohmann::json &j, const ToolPolicy &v);
void to_json(nlohmann::json &j, const NpcIdentity &v);
void to_json(nlohmann::json &j, const MemoryEntry &v);
void to_json(nlohmann::json &j, const NpcState &v);
void to_json(nlohmann::json &j, const NpcData &v);
void to_json(nlohmann::json &j, const FactData &v);
void to_json(nlohmann::json &j, const FlagData &v);
void to_json(nlohmann::json &j, const ConditionData &v);
void to_json(nlohmann::json &j, const EventActionData &v);
void to_json(nlohmann::json &j, const EventTriggerData &v);
void to_json(nlohmann::json &j, const PlayerState &v);
void to_json(nlohmann::json &j, const ItemPosition &v);
void to_json(nlohmann::json &j, const ClockState &v);
void to_json(nlohmann::json &j, const WorldState &v);

} // namespace chronicle
