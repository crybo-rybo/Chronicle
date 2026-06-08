/**
 * @file tool_registry.cpp
 * @brief Implementation of @ref ToolRegistry — validation, queuing, and Harness registration.
 *
 * @details Implements all @c validate_*, @c register_*, and utility methods,
 * plus @ref ToolRegistry::register_tools which wires game tools onto a
 * @c zoo::Agent via lambdas that capture @c this.
 */

#include "ai/tool_registry.hpp"
#include "ai/harness_compat.hpp"
#include "common/parse_utils.hpp"
#include "diagnostics/logger.hpp"
#include "engine/mutation_checks.hpp"
#include "engine/text_utils.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

#if CHRONICLE_ENABLE_HARNESS
#include <zoo/Agent.hpp>
#endif

namespace chronicle {

namespace {

#if CHRONICLE_ENABLE_HARNESS
void log_tool_result(std::string_view tool_name, const std::string &result) {
    const auto level =
        result.starts_with("Error:") ? logging::Level::Warning : logging::Level::Info;
    logging::write(level, "tools", "tool=" + std::string(tool_name) + " result=\"" + result + "\"");
}

using Json = zoo::JsonValue;

Json string_property(std::string_view description) {
    return Json{{"type", "string"}, {"description", description}};
}

Json integer_property(std::string_view description) {
    return Json{{"type", "integer"}, {"description", description}};
}

Json boolean_property(std::string_view description) {
    return Json{{"type", "boolean"}, {"description", description}};
}

Json mood_property() {
    return Json{
        {"type", "string"}, {"description", "New NPC mood"}, {"enum", Json::array(kValidMoods)}};
}

Json object_schema(Json properties, Json required) {
    return Json{{"type", "object"},
                {"properties", std::move(properties)},
                {"required", std::move(required)},
                {"additionalProperties", false}};
}

zoo::Expected<std::string> json_string_arg(const Json &args, std::string_view name) {
    auto value = args.at(name);
    if (!value) {
        return std::unexpected(value.error());
    }
    return value->as_string();
}

zoo::Expected<int> json_int_arg(const Json &args, std::string_view name) {
    auto value = args.at(name);
    if (!value) {
        return std::unexpected(value.error());
    }
    auto parsed = value->as_int64();
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    if (*parsed < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
        *parsed > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
        return std::unexpected(
            zoo::Error{zoo::ErrorCode::ToolValidationFailed, "integer argument out of range"});
    }
    return static_cast<int>(*parsed);
}

zoo::Expected<bool> json_bool_arg(const Json &args, std::string_view name) {
    auto value = args.at(name);
    if (!value) {
        return std::unexpected(value.error());
    }
    return value->as_bool();
}
#endif

} // namespace

ToolRegistry::ToolRegistry(const World &world, MutationSink sink)
    : world_(world), sink_(std::move(sink)) {}

void ToolRegistry::emit(MutationRequest req) {
    logging::write(logging::Level::Debug, "tools",
                   "queueing mutation actor=" + req.actor_id +
                       " param_count=" + std::to_string(req.params.size()));
    if (sink_) {
        sink_(std::move(req));
    } else {
        pending_.push_back(std::move(req));
    }
}

std::optional<std::string> ToolRegistry::enqueue_if_applicable(MutationRequest req) {
    if (auto err = check_mutation(world_, req)) {
        return *err;
    }
    emit(std::move(req));
    return std::nullopt;
}

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
    return chronicle::is_valid_mood(mood);
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

static bool scope_allows(const std::vector<std::string> &scope, const std::string &id) {
    return scope.empty() || std::ranges::contains(scope, id);
}

std::optional<std::string> ToolRegistry::policy_tool_error(const std::string &npc_id,
                                                           std::string_view tool_name) const {
    const auto &policy = world_.npcs.at(npc_id).identity.tool_policy;
    if (!std::ranges::contains(policy.allowed_tools, std::string(tool_name))) {
        return "Error: NPC '" + npc_id + "' is not allowed to use tool '" + std::string(tool_name) +
               "'.";
    }
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::policy_item_error(const std::string &npc_id,
                                                           const std::string &item_id) const {
    const auto &policy = world_.npcs.at(npc_id).identity.tool_policy;
    if (!scope_allows(policy.allowed_items, item_id)) {
        return "Error: NPC '" + npc_id + "' is not allowed to use item '" + item_id + "'.";
    }
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::policy_fact_error(const std::string &npc_id,
                                                           const std::string &fact_id) const {
    const auto &policy = world_.npcs.at(npc_id).identity.tool_policy;
    if (!scope_allows(policy.allowed_facts, fact_id)) {
        return "Error: NPC '" + npc_id + "' is not allowed to use fact '" + fact_id + "'.";
    }
    return std::nullopt;
}

std::optional<std::string> ToolRegistry::policy_flag_error(const std::string &npc_id,
                                                           const std::string &flag_id) const {
    const auto &policy = world_.npcs.at(npc_id).identity.tool_policy;
    if (!scope_allows(policy.allowed_flags, flag_id)) {
        return "Error: NPC '" + npc_id + "' is not allowed to use flag '" + flag_id + "'.";
    }
    return std::nullopt;
}

std::optional<std::string>
ToolRegistry::policy_location_error(const std::string &npc_id,
                                    const std::string &location_id) const {
    const auto &policy = world_.npcs.at(npc_id).identity.tool_policy;
    if (!scope_allows(policy.allowed_locations, location_id)) {
        return "Error: NPC '" + npc_id + "' is not allowed to use location '" + location_id + "'.";
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Validation methods
// ---------------------------------------------------------------------------

ToolRegistry::ValidationResult ToolRegistry::validate_give_item(const std::string &npc_id,
                                                                const std::string &item_id) const {
    auto npc_it = world_.npcs.find(npc_id);
    if (npc_it == world_.npcs.end())
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (auto err = policy_tool_error(npc_id, "give_item"))
        return *err;
    if (auto err = policy_item_error(npc_id, item_id))
        return *err;
    if (!item_exists(item_id))
        return "Error: Item '" + item_id + "' does not exist.";
    const auto &inv = npc_it->second.state.inventory;
    if (!std::ranges::contains(inv, item_id)) {
        return "Error: NPC '" + npc_id + "' does not have item '" + item_id +
               "'. Inventory: " + format_inventory(inv);
    }
    return MutationRequest{MutationRequest::Type::GiveItemToPlayer,
                           MutationRequest::Source::Npc,
                           npc_id,
                           {{"item_id", item_id}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_take_item(const std::string &npc_id,
                                                                const std::string &item_id) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (auto err = policy_tool_error(npc_id, "take_item"))
        return *err;
    if (auto err = policy_item_error(npc_id, item_id))
        return *err;
    if (!item_exists(item_id))
        return "Error: Item '" + item_id + "' does not exist.";
    if (!player_has_item(item_id)) {
        const auto &inv = world_.player.inventory;
        return "Error: Player does not have item '" + item_id +
               "'. Inventory: " + format_inventory(inv);
    }
    if (is_key_item(item_id))
        return "Error: Item '" + item_id + "' is a key item and cannot be taken.";
    return MutationRequest{MutationRequest::Type::TakeItemFromPlayer,
                           MutationRequest::Source::Npc,
                           npc_id,
                           {{"item_id", item_id}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_update_mood(const std::string &npc_id,
                                                                  const std::string &mood) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (auto err = policy_tool_error(npc_id, "update_mood"))
        return *err;
    if (!is_valid_mood(mood))
        return "Error: '" + mood +
               "' is not a valid mood. Valid moods: neutral, suspicious, friendly, hostile, "
               "fearful, grieving.";
    return MutationRequest{MutationRequest::Type::UpdateNpcMood,
                           MutationRequest::Source::Npc,
                           npc_id,
                           {{"mood", mood}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_update_trust(const std::string &npc_id,
                                                                   int delta) const {
    auto npc_it = world_.npcs.find(npc_id);
    if (npc_it == world_.npcs.end())
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (auto err = policy_tool_error(npc_id, "update_trust"))
        return *err;

    int current = npc_it->second.state.trust_toward_player;
    int new_trust = std::clamp(current + delta, -100, 100);
    int clamped_delta = new_trust - current;

    return MutationRequest{MutationRequest::Type::UpdateNpcTrust,
                           MutationRequest::Source::Npc,
                           npc_id,
                           {{"delta", std::to_string(clamped_delta)}}};
}

ToolRegistry::ValidationResult
ToolRegistry::validate_move_npc(const std::string &npc_id, const std::string &location_id) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (auto err = policy_tool_error(npc_id, "move_self"))
        return *err;
    if (auto err = policy_location_error(npc_id, location_id))
        return *err;
    if (!location_exists(location_id))
        return "Error: Location '" + location_id + "' does not exist.";
    return MutationRequest{MutationRequest::Type::MoveNpc,
                           MutationRequest::Source::Npc,
                           npc_id,
                           {{"location_id", location_id}}};
}

ToolRegistry::ValidationResult
ToolRegistry::validate_reveal_knowledge(const std::string &npc_id,
                                        const std::string &fact_id) const {
    auto npc_it = world_.npcs.find(npc_id);
    if (npc_it == world_.npcs.end())
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (auto err = policy_tool_error(npc_id, "reveal_knowledge"))
        return *err;
    if (auto err = policy_fact_error(npc_id, fact_id))
        return *err;

    const auto &knowledge = npc_it->second.identity.knowledge;
    if (!std::ranges::contains(knowledge, fact_id))
        return "Error: NPC '" + npc_id + "' does not know fact '" + fact_id + "'.";

    return MutationRequest{MutationRequest::Type::RevealKnowledge,
                           MutationRequest::Source::Npc,
                           npc_id,
                           {{"fact_id", fact_id}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_add_memory(const std::string &npc_id,
                                                                 const std::string &summary,
                                                                 int importance) const {
    if (!npc_exists(npc_id))
        return "Error: NPC '" + npc_id + "' does not exist.";
    if (auto err = policy_tool_error(npc_id, "remember"))
        return *err;
    auto trimmed_summary = text::trim_copy(summary);
    if (trimmed_summary.empty())
        return "Error: Memory summary must not be empty.";

    int clamped = std::clamp(importance, 1, 10);
    return MutationRequest{
        MutationRequest::Type::AddMemory,
        MutationRequest::Source::Npc,
        npc_id,
        {{"summary", std::move(trimmed_summary)}, {"importance", std::to_string(clamped)}}};
}

ToolRegistry::ValidationResult ToolRegistry::validate_set_flag(const std::string &flag_id,
                                                               bool value) const {
    if (!flag_exists(flag_id))
        return "Error: Flag '" + flag_id + "' does not exist in world flags.";
    return MutationRequest{MutationRequest::Type::SetFlag,
                           MutationRequest::Source::Npc,
                           "",
                           {{"flag_id", flag_id}, {"value", value ? "true" : "false"}}};
}

// ---------------------------------------------------------------------------
// Register methods: validate + enqueue
// ---------------------------------------------------------------------------

std::optional<std::string> ToolRegistry::register_give_item(const std::string &npc_id,
                                                            const std::string &item_id) {
    auto result = validate_give_item(npc_id, item_id);
    if (auto *error = std::get_if<std::string>(&result)) {
        return *error;
    }
    return enqueue_if_applicable(std::get<MutationRequest>(result));
}

std::optional<std::string> ToolRegistry::register_take_item(const std::string &npc_id,
                                                            const std::string &item_id) {
    auto result = validate_take_item(npc_id, item_id);
    if (auto *error = std::get_if<std::string>(&result)) {
        return *error;
    }
    return enqueue_if_applicable(std::get<MutationRequest>(result));
}

std::optional<std::string> ToolRegistry::register_update_mood(const std::string &npc_id,
                                                              const std::string &mood) {
    auto result = validate_update_mood(npc_id, mood);
    if (auto *error = std::get_if<std::string>(&result)) {
        return *error;
    }
    return enqueue_if_applicable(std::get<MutationRequest>(result));
}

std::optional<std::string> ToolRegistry::register_update_trust(const std::string &npc_id,
                                                               int delta) {
    auto result = validate_update_trust(npc_id, delta);
    if (auto *error = std::get_if<std::string>(&result)) {
        return *error;
    }
    return enqueue_if_applicable(std::get<MutationRequest>(result));
}

std::optional<std::string> ToolRegistry::register_move_npc(const std::string &npc_id,
                                                           const std::string &location_id) {
    auto result = validate_move_npc(npc_id, location_id);
    if (auto *error = std::get_if<std::string>(&result)) {
        return *error;
    }
    return enqueue_if_applicable(std::get<MutationRequest>(result));
}

std::optional<std::string> ToolRegistry::register_reveal_knowledge(const std::string &npc_id,
                                                                   const std::string &fact_id) {
    auto result = validate_reveal_knowledge(npc_id, fact_id);
    if (auto *error = std::get_if<std::string>(&result)) {
        return *error;
    }
    return enqueue_if_applicable(std::get<MutationRequest>(result));
}

std::optional<std::string> ToolRegistry::register_add_memory(const std::string &npc_id,
                                                             const std::string &summary,
                                                             int importance) {
    auto result = validate_add_memory(npc_id, summary, importance);
    if (auto *error = std::get_if<std::string>(&result)) {
        return *error;
    }
    return enqueue_if_applicable(std::get<MutationRequest>(result));
}

std::optional<std::string> ToolRegistry::register_set_flag(const std::string &flag_id, bool value) {
    auto result = validate_set_flag(flag_id, value);
    if (auto *error = std::get_if<std::string>(&result)) {
        return *error;
    }
    return enqueue_if_applicable(std::get<MutationRequest>(result));
}

std::string ToolRegistry::handle_take_item_tool(const std::string &npc_id,
                                                const std::string &item_id) {
    if (auto err = register_take_item(npc_id, item_id)) {
        return *err;
    }
    return "OK. You received: " + format_item_details(item_id);
}

std::string ToolRegistry::handle_inspect_item_tool(const std::string &npc_id,
                                                   const std::string &item_id) {
    if (!item_exists(item_id))
        return "Error: Item '" + item_id + "' does not exist.";
    if (auto err = policy_tool_error(npc_id, "inspect_item"))
        return *err;
    if (auto err = policy_item_error(npc_id, item_id))
        return *err;
    if (!npc_has_item(npc_id, item_id)) {
        const auto &inv = world_.npcs.at(npc_id).state.inventory;
        return "Error: You do not have item '" + item_id +
               "'. Your inventory: " + format_inventory(inv);
    }
    return format_item_details(item_id);
}

std::string ToolRegistry::handle_set_flag_tool(const std::string &flag_id,
                                               const std::string &value_str) {
    auto value = parse_bool(value_str);
    if (!value) {
        return "Error: flag value must be 'true', 'false', '1', or '0', got '" + value_str + "'.";
    }
    if (!active_npc_id_.empty()) {
        if (auto err = policy_tool_error(active_npc_id_, "set_flag")) {
            return *err;
        }
        if (auto err = policy_flag_error(active_npc_id_, flag_id)) {
            return *err;
        }
    }
    if (auto err = register_set_flag(flag_id, *value)) {
        return *err;
    }
    return "OK";
}

std::optional<std::string> ToolRegistry::register_say(const std::string &npc_id,
                                                      const std::string &dialogue) {
    if (npc_exists(npc_id)) {
        if (auto err = policy_tool_error(npc_id, "say")) {
            logging::write(logging::Level::Warning, "tools", *err);
            return *err;
        }
    }
    logging::write(logging::Level::Info, "tools",
                   "tool=say npc=" + npc_id + " dialogue=\"" + dialogue + "\"");
    dialogue_log_.emplace_back(npc_id, dialogue);
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> ToolRegistry::drain_dialogue_log() {
    auto dialogue = std::move(dialogue_log_);
    dialogue_log_.clear();
    return dialogue;
}

const std::vector<MutationRequest> &ToolRegistry::pending_mutations() const {
    return pending_;
}

void ToolRegistry::clear_pending() {
    pending_.clear();
}

void ToolRegistry::clear_dialogue_log() {
    dialogue_log_.clear();
}

void ToolRegistry::clear_all() {
    clear_pending();
    clear_dialogue_log();
}

void ToolRegistry::set_active_npc_id(std::string npc_id) {
    logging::write(logging::Level::Debug, "tools", "active_npc_id=" + npc_id);
    active_npc_id_ = std::move(npc_id);
}

std::string ToolRegistry::format_item_details(const std::string &item_id) const {
    const auto &item = world_.items.at(item_id);
    std::string details = item.name + ": " + item.description;

    auto readable_it = item.properties.find("readable");
    if (readable_it != item.properties.end() && readable_it->second == "true") {
        auto text_it = item.properties.find("text");
        if (text_it != item.properties.end()) {
            details += " It reads: \"" + text_it->second + "\"";
        }
    }
    return details;
}

#if CHRONICLE_ENABLE_HARNESS
void ToolRegistry::register_harness_tools(zoo::Agent::Builder &builder) {
    logging::write(logging::Level::Info, "tools", "registering Chronicle harness tools");

    builder.tool(
        zoo::ToolSpec{"give_item", "Give an item to the player",
                      object_schema(Json{{"item_id", string_property("Item ID to give")}},
                                    Json::array({"item_id"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto item_id = json_string_arg(args, "item_id");
            if (!item_id) {
                return std::unexpected(item_id.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=give_item npc=" + active_npc_id_ + " item_id=" + *item_id);
            if (auto err = register_give_item(active_npc_id_, *item_id)) {
                log_tool_result("give_item", *err);
                return *err;
            }
            log_tool_result("give_item", "OK");
            return "OK";
        });

    builder.tool(
        zoo::ToolSpec{"take_item", "Take an item from the player",
                      object_schema(Json{{"item_id", string_property("Item ID to take")}},
                                    Json::array({"item_id"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto item_id = json_string_arg(args, "item_id");
            if (!item_id) {
                return std::unexpected(item_id.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=take_item npc=" + active_npc_id_ + " item_id=" + *item_id);
            auto result = handle_take_item_tool(active_npc_id_, *item_id);
            log_tool_result("take_item", result);
            return result;
        });

    builder.tool(
        zoo::ToolSpec{"update_mood", "Change the NPC's current mood",
                      object_schema(Json{{"mood", mood_property()}}, Json::array({"mood"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto mood = json_string_arg(args, "mood");
            if (!mood) {
                return std::unexpected(mood.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=update_mood npc=" + active_npc_id_ + " mood=" + *mood);
            if (auto err = register_update_mood(active_npc_id_, *mood)) {
                log_tool_result("update_mood", *err);
                return *err;
            }
            log_tool_result("update_mood", "OK");
            return "OK";
        });

    builder.tool(
        zoo::ToolSpec{
            "update_trust", "Change the player's trust level with this NPC",
            object_schema(Json{{"delta", integer_property("Trust delta from -100 to 100")}},
                          Json::array({"delta"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto delta = json_int_arg(args, "delta");
            if (!delta) {
                return std::unexpected(delta.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=update_trust npc=" + active_npc_id_ +
                               " delta=" + std::to_string(*delta));
            if (auto err = register_update_trust(active_npc_id_, *delta)) {
                log_tool_result("update_trust", *err);
                return *err;
            }
            log_tool_result("update_trust", "OK");
            return "OK";
        });

    builder.tool(
        zoo::ToolSpec{
            "move_self", "Move the NPC to a new location",
            object_schema(Json{{"location_id", string_property("Destination location ID")}},
                          Json::array({"location_id"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto location_id = json_string_arg(args, "location_id");
            if (!location_id) {
                return std::unexpected(location_id.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=move_self npc=" + active_npc_id_ + " location_id=" + *location_id);
            if (auto err = register_move_npc(active_npc_id_, *location_id)) {
                log_tool_result("move_self", *err);
                return *err;
            }
            log_tool_result("move_self", "OK");
            return "OK";
        });

    builder.tool(
        zoo::ToolSpec{"reveal_knowledge", "Reveal a known fact to the player",
                      object_schema(Json{{"fact_id", string_property("Fact ID to reveal")}},
                                    Json::array({"fact_id"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto fact_id = json_string_arg(args, "fact_id");
            if (!fact_id) {
                return std::unexpected(fact_id.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=reveal_knowledge npc=" + active_npc_id_ + " fact_id=" + *fact_id);
            if (auto err = register_reveal_knowledge(active_npc_id_, *fact_id)) {
                log_tool_result("reveal_knowledge", *err);
                return *err;
            }
            log_tool_result("reveal_knowledge", "OK");
            return "OK";
        });

    builder.tool(
        zoo::ToolSpec{
            "remember",
            "Create a durable NPC memory only for future-relevant details such as clues, promises, "
            "commitments, relationship changes, or major emotional shifts. Do not use this for "
            "greetings, filler, or every player message.",
            object_schema(Json{{"summary", string_property("Concise memory summary")},
                               {"importance", integer_property("Importance from 1 to 10")}},
                          Json::array({"summary", "importance"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto summary = json_string_arg(args, "summary");
            if (!summary) {
                return std::unexpected(summary.error());
            }
            auto importance = json_int_arg(args, "importance");
            if (!importance) {
                return std::unexpected(importance.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=remember npc=" + active_npc_id_ + " importance=" +
                               std::to_string(*importance) + " summary=\"" + *summary + "\"");
            if (auto err = register_add_memory(active_npc_id_, *summary, *importance)) {
                log_tool_result("remember", *err);
                return *err;
            }
            log_tool_result("remember", "OK");
            return "OK";
        });

    builder.tool(
        zoo::ToolSpec{"set_flag", "Set or update a world narrative flag",
                      object_schema(Json{{"flag_id", string_property("Flag ID to set")},
                                         {"value", boolean_property("New flag value")}},
                                    Json::array({"flag_id", "value"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto flag_id = json_string_arg(args, "flag_id");
            if (!flag_id) {
                return std::unexpected(flag_id.error());
            }
            auto value = json_bool_arg(args, "value");
            if (!value) {
                return std::unexpected(value.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=set_flag flag_id=" + *flag_id +
                               " value=" + (*value ? "true" : "false"));
            auto result = handle_set_flag_tool(*flag_id, *value ? "true" : "false");
            log_tool_result("set_flag", result);
            return result;
        });

    builder.tool(
        zoo::ToolSpec{"inspect_item", "Examine an item in your inventory",
                      object_schema(Json{{"item_id", string_property("Item ID to inspect")}},
                                    Json::array({"item_id"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto item_id = json_string_arg(args, "item_id");
            if (!item_id) {
                return std::unexpected(item_id.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=inspect_item npc=" + active_npc_id_ + " item_id=" + *item_id);
            auto result = handle_inspect_item_tool(active_npc_id_, *item_id);
            log_tool_result("inspect_item", result);
            return result;
        });

    builder.tool(
        zoo::ToolSpec{"say", "Speak dialogue to the player",
                      object_schema(Json{{"dialogue", string_property("Dialogue text to speak")}},
                                    Json::array({"dialogue"}))},
        [this](const Json &args, const zoo::tools::ToolContext &) -> zoo::Expected<std::string> {
            auto dialogue = json_string_arg(args, "dialogue");
            if (!dialogue) {
                return std::unexpected(dialogue.error());
            }
            logging::write(logging::Level::Info, "tools",
                           "tool=say npc=" + active_npc_id_ + " dialogue=\"" + *dialogue + "\"");
            if (auto err = register_say(active_npc_id_, *dialogue)) {
                log_tool_result("say", *err);
                return *err;
            }
            log_tool_result("say", "OK");
            return "OK";
        });
}
#endif

} // namespace chronicle
