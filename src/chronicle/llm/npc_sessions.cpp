#include "chronicle/llm/npc_sessions.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <scry/config.hpp>
#include <scry/conversation.hpp>
#include <scry/harness.hpp>
#include <scry/reflection.hpp>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "chronicle/prompt.hpp"

namespace chronicle {

namespace {

// Reflected tool results deliberately use a successful transport result for
// policy rejections. That makes the exact rejection visible to the model so it
// can choose another action instead of seeing Scry's generic handler failure.
struct ToolAck {
    [[= scry::reflection::description{"Whether Chronicle accepted the action"}]] bool ok;
    [[= scry::reflection::description{"Action result when ok is true"}]] std::string result;
    [[= scry::reflection::description{
        "Exact rejection reason when ok is false"}]] std::string error;
};

namespace reflected {

struct Say {
    [[= scry::reflection::description{"Dialogue to speak aloud to the player"}]] std::string text;
    [[nodiscard]] tools::Say into_call() && { return {.text = std::move(text)}; }
};

struct GiveItem {
    [[= scry::reflection::description{
        "Authored id of an item you currently hold"}]] std::string item_id;
    [[nodiscard]] tools::GiveItem into_call() && { return {.item_id = std::move(item_id)}; }
};

struct TakeItem {
    [[= scry::reflection::description{
        "Authored id of an item the player currently holds"}]] std::string item_id;
    [[nodiscard]] tools::TakeItem into_call() && { return {.item_id = std::move(item_id)}; }
};

struct UpdateMood {
    [[= scry::reflection::description{
        "Your new mood from the fixed mood vocabulary"}]] tools::Mood mood;
    [[nodiscard]] tools::UpdateMood into_call() && { return {.mood = mood}; }
};

struct UpdateTrust {
    [[= scry::reflection::description{
        "Signed trust adjustment from -100 to 100"}]] std::int16_t delta;
    [[nodiscard]] tools::UpdateTrust into_call() && { return {.delta = static_cast<int>(delta)}; }
};

struct MoveSelf {
    [[= scry::reflection::description{
        "Authored id of an allowed destination location"}]] std::string location_id;
    [[nodiscard]] tools::MoveSelf into_call() && { return {.location_id = std::move(location_id)}; }
};

struct RevealKnowledge {
    [[= scry::reflection::description{
        "Authored id of a fact you know and may reveal"}]] std::string fact_id;
    [[nodiscard]] tools::RevealKnowledge into_call() && { return {.fact_id = std::move(fact_id)}; }
};

struct Remember {
    [[= scry::reflection::description{
        "Short factual memory of this conversation"}]] std::string summary;
    [[= scry::reflection::description{
        "Importance from 1 (minor) to 10 (critical)"}]] std::uint16_t importance{5};
    [[nodiscard]] tools::Remember into_call() && {
        return {.summary = std::move(summary), .importance = static_cast<int>(importance)};
    }
};

struct SetFlag {
    [[= scry::reflection::description{
        "Authored id of a flag allowed by your policy"}]] std::string flag_id;
    [[= scry::reflection::description{"Explicit value to store in the flag"}]] bool value;
    [[nodiscard]] tools::SetFlag into_call() && {
        return {.flag_id = std::move(flag_id), .value = value};
    }
};

struct InspectItem {
    [[= scry::reflection::description{
        "Authored id of an item you are allowed to inspect"}]] std::string item_id;
    [[nodiscard]] tools::InspectItem into_call() && { return {.item_id = std::move(item_id)}; }
};

} // namespace reflected

template <typename Arguments, typename DomainCall> struct ToolBinding {
    using arguments_type = Arguments;
    using domain_call_type = DomainCall;
    std::string_view name;
    std::string_view description;
};

template <typename Arguments, typename DomainCall>
constexpr ToolBinding<Arguments, DomainCall> bind(const std::string_view description) {
    return {.name = DomainCall::name, .description = description};
}

constexpr auto TOOL_BINDINGS = std::tuple{
    bind<reflected::Say, tools::Say>("Speak aloud to the player."),
    bind<reflected::GiveItem, tools::GiveItem>("Give an item you hold to the player."),
    bind<reflected::TakeItem, tools::TakeItem>("Take an item from the player."),
    bind<reflected::UpdateMood, tools::UpdateMood>("Change your mood."),
    bind<reflected::UpdateTrust, tools::UpdateTrust>("Adjust trust toward the player."),
    bind<reflected::MoveSelf, tools::MoveSelf>("Move to another allowed location."),
    bind<reflected::RevealKnowledge, tools::RevealKnowledge>("Reveal an authored fact you know."),
    bind<reflected::Remember, tools::Remember>("Store a short memory about this conversation."),
    bind<reflected::SetFlag, tools::SetFlag>("Set an authored narrative flag."),
    bind<reflected::InspectItem, tools::InspectItem>("Inspect an item without changing the world."),
};

constexpr auto TOOL_BINDING_NAMES =
    std::apply([](const auto &...binding) { return std::array{binding.name...}; }, TOOL_BINDINGS);

consteval bool binds_exact_tool_catalog() {
    if (TOOL_BINDING_NAMES.size() != tools::NPC_TOOL_NAMES.size()) {
        return false;
    }
    return std::ranges::all_of(tools::NPC_TOOL_NAMES, [](const std::string_view expected) {
        return std::ranges::count(TOOL_BINDING_NAMES, expected) == 1;
    });
}

static_assert(binds_exact_tool_catalog(),
              "Every Chronicle NPC tool must have exactly one reflected Scry binding");

template <typename Function> void for_each_tool_binding(Function &&function) {
    std::apply([&](const auto &...binding) { (function(binding), ...); }, TOOL_BINDINGS);
}

constexpr std::size_t MAX_LIVE_NPC_SESSIONS = 16;

class SessionError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

std::string strip_copy(const std::string &text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string env_text(const char *name) {
    const char *value = std::getenv(name);
    return value == nullptr ? "" : strip_copy(value);
}

bool env_truthy(const char *name) {
    const std::string value = env_text(name);
    return value == "1" || value == "true" || value == "yes";
}

bool closes_committed_turn(const nlohmann::json &message) {
    if (!message.is_object() || message.value("role", "") != "assistant") {
        return false;
    }
    const auto content = message.find("content");
    if (content == message.end() || !content->is_array()) {
        return false;
    }
    return std::ranges::none_of(*content, [](const nlohmann::json &block) {
        return block.is_object() && block.value("type", "") == "tool_call";
    });
}

void trim_conversation_history(scry::Conversation &conversation, const int token_budget) {
    auto encoded = conversation.to_json();
    if (!encoded) {
        throw SessionError("Could not inspect conversation history: " + encoded.error().message);
    }
    auto document = nlohmann::json::parse(encoded->text, nullptr, false);
    if (document.is_discarded() || !document.contains("messages") ||
        !document["messages"].is_array()) {
        throw SessionError("Could not inspect conversation history: invalid conversation JSON");
    }

    auto &messages = document["messages"];
    const auto byte_budget = static_cast<std::size_t>(std::max(1, token_budget)) * 4U;
    bool changed = false;
    while (!messages.empty() && messages.dump().size() > byte_budget) {
        std::size_t erase_count = messages.size();
        for (std::size_t index = 0; index < messages.size(); ++index) {
            if (closes_committed_turn(messages[index])) {
                erase_count = index + 1;
                break;
            }
        }
        messages.erase(messages.begin(),
                       messages.begin() +
                           static_cast<nlohmann::json::difference_type>(erase_count));
        changed = true;
    }
    if (!changed) {
        return;
    }

    auto compacted = scry::Conversation::from_json(scry::Json{.text = document.dump()});
    if (!compacted) {
        throw SessionError("Could not compact conversation history: " + compacted.error().message);
    }
    conversation = std::move(*compacted);
}

} // namespace

std::optional<EndpointConfig> resolve_endpoint(const std::optional<std::string> &base_url_flag,
                                               const std::optional<std::string> &model_flag,
                                               const ConfigData *cartridge_config) {
    EndpointConfig endpoint;
    endpoint.base_url = base_url_flag ? strip_copy(*base_url_flag) : env_text("CHRONICLE_BASE_URL");
    endpoint.model = model_flag ? strip_copy(*model_flag) : env_text("CHRONICLE_MODEL");
    if (cartridge_config != nullptr) {
        endpoint.temperature = cartridge_config->temperature;
        endpoint.max_tokens = cartridge_config->max_response_tokens;
        endpoint.timeout_ms = cartridge_config->inference_timeout_ms;
    }
    endpoint.anthropic_dialect = env_text("CHRONICLE_DIALECT") == "anthropic";
    endpoint.disable_reasoning = env_truthy("CHRONICLE_DISABLE_REASONING");

    if (!endpoint.configured()) {
        return std::nullopt;
    }
    std::string key = env_text("CHRONICLE_API_KEY");
    if (key.empty()) {
        key = env_text("OPENAI_API_KEY");
    }
    if (!key.empty()) {
        endpoint.api_key = std::move(key);
    }
    return endpoint;
}

std::vector<ReflectedToolSchema> npc_tool_schemas() {
    std::vector<ReflectedToolSchema> schemas;
    schemas.reserve(std::tuple_size_v<decltype(TOOL_BINDINGS)>);
    for_each_tool_binding([&]<typename Binding>(const Binding &binding) {
        using Arguments = typename Binding::arguments_type;
        schemas.push_back(
            {.name = std::string(binding.name),
             .description = std::string(binding.description),
             .input_schema = std::string(scry::reflection::input_schema_v<Arguments>)});
    });
    return schemas;
}

struct NpcSessionManager::Session {
    scry::Harness harness;
    scry::Conversation conversation;
    std::uint64_t last_used = 0;
};

NpcSessionManager::NpcSessionManager(CartridgeGame &game, EndpointConfig endpoint)
    : game_(game), endpoint_(std::move(endpoint)) {}

NpcSessionManager::~NpcSessionManager() = default;

NpcSessionManager::Session &NpcSessionManager::get_or_create(const std::string &npc_id) {
    if (const auto it = sessions_.find(npc_id); it != sessions_.end()) {
        it->second->last_used = ++use_sequence_;
        return *it->second;
    }

    const auto npc_it = game_.world().npcs.find(npc_id);
    if (npc_it == game_.world().npcs.end()) {
        throw SessionError("Unknown NPC: " + npc_id);
    }

    auto created = scry::Harness::create(scry::Config{
        .base_url = endpoint_.base_url,
        .api_key = endpoint_.api_key,
        .model = endpoint_.model,
        .dialect = endpoint_.anthropic_dialect ? scry::ProviderDialect::anthropic
                                               : scry::ProviderDialect::openai_compatible,
        .sampling = {.temperature = endpoint_.temperature,
                     .top_p = {},
                     .max_tokens = static_cast<std::uint32_t>(std::max(1, endpoint_.max_tokens))},
        .reasoning_mode = endpoint_.disable_reasoning ? scry::ReasoningMode::disabled
                                                      : scry::ReasoningMode::provider_default,
        .timeouts = {.transfer = std::chrono::milliseconds(std::max(1, endpoint_.timeout_ms))},
        .limits = {.max_pending_turns = 1, .max_tool_arguments_bytes = std::size_t{64} * 1024},
        .max_tool_rounds = 8,
    });
    if (!created) {
        throw SessionError(created.error().message);
    }
    auto harness = std::move(*created);

    // Register exactly the tools this NPC's cartridge policy allows; the
    // schema for each comes from the argument aggregate via reflection.
    const auto dispatch = [this, npc_id](tools::NpcToolCall call) -> ToolAck {
        auto outcome = game_.submit_npc_tool(npc_id, call);
        if (!outcome) {
            return ToolAck{.ok = false, .result = {}, .error = outcome.error().reason};
        }
        std::string ack = "ok";
        for (auto &event : *outcome) {
            if (event.kind == EventKind::dialogue) {
                turn_had_dialogue_ = true;
            }
            if (event.kind == EventKind::tool_result) {
                ack = event.text;
            }
            turn_events_.push_back(event);
        }
        return ToolAck{.ok = true, .result = std::move(ack), .error = {}};
    };

    std::vector<std::string> registered;
    for (const auto &name : npc_it->second.identity.tool_policy.allowed_tools) {
        if (std::ranges::find(registered, name) != registered.end()) {
            continue;
        }
        registered.push_back(name);
        bool found = false;
        for_each_tool_binding([&]<typename Binding>(const Binding &binding) {
            if (binding.name != name) {
                return;
            }
            found = true;
            using Arguments = typename Binding::arguments_type;
            using DomainCall = typename Binding::domain_call_type;
            static_assert(
                std::is_same_v<decltype(std::declval<Arguments &&>().into_call()), DomainCall>);
            auto status = scry::reflection::add<Arguments>(
                harness.tools(),
                {.name = std::string(binding.name),
                 .description = std::string(binding.description)},
                [dispatch](Arguments args) { return dispatch(std::move(args).into_call()); });
            if (!status) {
                throw SessionError("Tool registration failed for '" + name +
                                   "': " + status.error().message);
            }
        });
        if (!found) {
            throw SessionError("Unknown tool in validated policy: " + name);
        }
    }

    // Restore the saved conversation when one is staged; otherwise start
    // fresh with the NPC's static system prompt.
    std::optional<scry::Conversation> conversation;
    if (const auto staged = pending_restore_.find(npc_id); staged != pending_restore_.end()) {
        auto restored = scry::Conversation::from_json(scry::Json{.text = staged->second.dump()});
        pending_restore_.erase(staged);
        if (!restored) {
            throw SessionError("Saved conversation for '" + npc_id +
                               "' is invalid: " + restored.error().message);
        }
        conversation = std::move(*restored);
    }
    if (!conversation) {
        auto fresh = scry::Conversation::create(
            {.system_prompt = build_npc_system_prompt(game_.world(), npc_id)});
        if (!fresh) {
            throw SessionError(fresh.error().message);
        }
        conversation = std::move(*fresh);
    }

    if (sessions_.size() >= MAX_LIVE_NPC_SESSIONS) {
        const auto victim = std::ranges::min_element(
            sessions_, {}, [](const auto &entry) { return entry.second->last_used; });
        auto document = victim->second->conversation.to_json();
        if (!document) {
            throw SessionError("Could not suspend NPC session: " + document.error().message);
        }
        auto parsed = nlohmann::json::parse(document->text, nullptr, false);
        if (parsed.is_discarded()) {
            throw SessionError("Could not suspend NPC session: invalid conversation JSON");
        }
        pending_restore_[victim->first] = std::move(parsed);
        sessions_.erase(victim);
    }

    auto session =
        std::make_unique<Session>(std::move(harness), std::move(*conversation), ++use_sequence_);
    auto inserted = sessions_.emplace(npc_id, std::move(session)).first;
    return *inserted->second;
}

NpcTurnResult NpcSessionManager::run_turn(const std::string &npc_id,
                                          const std::string &player_text) {
    turn_events_.clear();
    turn_had_dialogue_ = false;

    Session *session = nullptr;
    try {
        session = &get_or_create(npc_id);
        trim_conversation_history(session->conversation, game_.world().config.max_history_tokens);
    } catch (const std::exception &exc) {
        return std::unexpected(NpcTurnFailure{.message = exc.what(), .world_rolled_back = true});
    }

    const std::string message = build_npc_turn_message(game_.world(), npc_id, player_text);
    auto checkpoint = game_.checkpoint_runtime();
    auto result = session->harness.send_and_wait(session->conversation, message);

    GameEvents events = std::move(turn_events_);
    turn_events_.clear();
    if (!result) {
        events.clear();
        auto restored = game_.restore_runtime(std::move(checkpoint));
        std::string message_text = result.error().message;
        const bool rolled_back = restored.has_value();
        if (!rolled_back) {
            message_text += "; rollback failed: " + restored.error().reason;
        }
        return std::unexpected(
            NpcTurnFailure{.message = std::move(message_text), .world_rolled_back = rolled_back});
    }

    const std::string final_text = strip_copy(result->text);
    if (!final_text.empty() && !turn_had_dialogue_) {
        const auto &npc = game_.world().npcs.at(npc_id);
        events.push_back({EventKind::dialogue, npc.identity.name + ": \"" + final_text + "\""});
    }
    return events;
}

std::expected<nlohmann::json, std::string> NpcSessionManager::snapshot_conversations() {
    nlohmann::json snapshot = nlohmann::json::object();
    for (const auto &[npc_id, document] : pending_restore_) {
        snapshot[npc_id] = document;
    }
    for (auto &[npc_id, session] : sessions_) {
        auto doc = session->conversation.to_json();
        if (!doc) {
            return std::unexpected("could not serialize conversation for '" + npc_id +
                                   "': " + doc.error().message);
        }
        auto parsed = nlohmann::json::parse(doc->text, nullptr, false);
        if (parsed.is_discarded()) {
            return std::unexpected("conversation for '" + npc_id + "' serialized to invalid JSON");
        }
        snapshot[npc_id] = std::move(parsed);
    }
    return snapshot;
}

std::expected<void, std::string>
NpcSessionManager::restore_conversations(const nlohmann::json &conversations) {
    if (!conversations.is_object()) {
        return std::unexpected("conversations must be an object");
    }
    if (conversations.size() > game_.world().npcs.size()) {
        return std::unexpected("save contains too many NPC conversations");
    }

    std::map<std::string, nlohmann::json> staged;
    for (const auto &[npc_id, doc] : conversations.items()) {
        if (!game_.world().npcs.contains(npc_id)) {
            return std::unexpected("conversation references unknown NPC '" + npc_id + "'");
        }
        if (!doc.is_object() || !doc.contains("system_prompt") ||
            !doc["system_prompt"].is_string()) {
            return std::unexpected("conversation for '" + npc_id + "' is malformed");
        }
        if (doc["system_prompt"].get_ref<const std::string &>() !=
            build_npc_system_prompt(game_.world(), npc_id)) {
            return std::unexpected("conversation for '" + npc_id +
                                   "' has a non-canonical system prompt");
        }

        auto restored = scry::Conversation::from_json(scry::Json{.text = doc.dump()});
        if (!restored) {
            return std::unexpected("conversation for '" + npc_id +
                                   "' is invalid: " + restored.error().message);
        }
        auto canonical = restored->to_json();
        if (!canonical) {
            return std::unexpected("conversation for '" + npc_id +
                                   "' could not be normalized: " + canonical.error().message);
        }
        auto normalized = nlohmann::json::parse(canonical->text, nullptr, false);
        if (normalized.is_discarded()) {
            return std::unexpected("conversation for '" + npc_id + "' could not be normalized");
        }
        staged.emplace(npc_id, std::move(normalized));
    }

    sessions_.clear();
    pending_restore_ = std::move(staged);
    return {};
}

} // namespace chronicle
