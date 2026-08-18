#include "chronicle/llm/npc_sessions.hpp"

#include <algorithm>
#include <cstdlib>
#include <scry/config.hpp>
#include <scry/conversation.hpp>
#include <scry/harness.hpp>
#include <scry/reflection.hpp>
#include <stdexcept>

#include "chronicle/prompt.hpp"

namespace chronicle {

namespace {

// Reflected result returned to the model by every NPC tool handler.
struct ToolAck {
    std::string result;
};

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

struct NpcSessionManager::Session {
    scry::Harness harness;
    scry::Conversation conversation;
};

NpcSessionManager::NpcSessionManager(CartridgeGame &game, EndpointConfig endpoint)
    : game_(game), endpoint_(std::move(endpoint)) {}

NpcSessionManager::~NpcSessionManager() = default;

NpcSessionManager::Session &NpcSessionManager::get_or_create(const std::string &npc_id) {
    if (const auto it = sessions_.find(npc_id); it != sessions_.end()) {
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
    });
    if (!created) {
        throw SessionError(created.error().message);
    }
    auto harness = std::move(*created);

    // Register exactly the tools this NPC's cartridge policy allows; the
    // schema for each comes from the argument aggregate via reflection.
    const auto dispatch = [this, npc_id](tools::NpcToolCall call) -> scry::Result<ToolAck> {
        auto outcome = game_.submit_npc_tool(npc_id, call);
        if (!outcome) {
            return std::unexpected(scry::Error{.category = scry::ErrorCategory::tool,
                                               .message = outcome.error().reason});
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
        return ToolAck{.result = std::move(ack)};
    };

    const auto add_tool = [&]<typename Args>(const std::string &name, std::type_identity<Args>) {
        auto status = scry::reflection::add<Args>(
            harness.tools(),
            {.name = name, .description = std::string(tools::tool_description(name))},
            [dispatch](Args args) { return dispatch(tools::NpcToolCall{std::move(args)}); });
        if (!status) {
            throw SessionError("Tool registration failed for '" + name +
                               "': " + status.error().message);
        }
    };

    std::vector<std::string> registered;
    for (const auto &name : npc_it->second.identity.tool_policy.allowed_tools) {
        if (std::ranges::find(registered, name) != registered.end()) {
            continue;
        }
        registered.push_back(name);
        if (name == "say") {
            add_tool(name, std::type_identity<tools::Say>{});
        } else if (name == "give_item") {
            add_tool(name, std::type_identity<tools::GiveItem>{});
        } else if (name == "take_item") {
            add_tool(name, std::type_identity<tools::TakeItem>{});
        } else if (name == "update_mood") {
            add_tool(name, std::type_identity<tools::UpdateMood>{});
        } else if (name == "update_trust") {
            add_tool(name, std::type_identity<tools::UpdateTrust>{});
        } else if (name == "move_self") {
            add_tool(name, std::type_identity<tools::MoveSelf>{});
        } else if (name == "reveal_knowledge") {
            add_tool(name, std::type_identity<tools::RevealKnowledge>{});
        } else if (name == "remember") {
            add_tool(name, std::type_identity<tools::Remember>{});
        } else if (name == "set_flag") {
            add_tool(name, std::type_identity<tools::SetFlag>{});
        } else if (name == "inspect_item") {
            add_tool(name, std::type_identity<tools::InspectItem>{});
        }
        // Unknown names were already flagged by cartridge validation.
    }

    // Restore the saved conversation when one is staged; otherwise start
    // fresh with the NPC's static system prompt.
    std::optional<scry::Conversation> conversation;
    if (const auto staged = pending_restore_.find(npc_id); staged != pending_restore_.end()) {
        auto restored = scry::Conversation::from_json(scry::Json{.text = staged->second.dump()});
        pending_restore_.erase(staged);
        if (restored) {
            conversation = std::move(*restored);
        }
    }
    if (!conversation) {
        auto fresh = scry::Conversation::create(
            {.system_prompt = build_npc_system_prompt(game_.world(), npc_id)});
        if (!fresh) {
            throw SessionError(fresh.error().message);
        }
        conversation = std::move(*fresh);
    }

    auto session = std::make_unique<Session>(std::move(harness), std::move(*conversation));
    auto [inserted, ok] = sessions_.emplace(npc_id, std::move(session));
    return *inserted->second;
}

GameEvents NpcSessionManager::run_turn(const std::string &npc_id, const std::string &player_text) {
    turn_events_.clear();
    turn_had_dialogue_ = false;

    const auto degrade = [](const std::string &message) -> GameEvents {
        return {{EventKind::warning, "Inference failed (" + message +
                                         "). The conversation falters, but the world remains."}};
    };

    Session *session = nullptr;
    try {
        session = &get_or_create(npc_id);
    } catch (const std::exception &exc) {
        return degrade(exc.what());
    }

    const std::string message = build_npc_turn_message(game_.world(), npc_id, player_text);
    auto result = session->harness.send_and_wait(session->conversation, message);

    GameEvents events = std::move(turn_events_);
    turn_events_.clear();
    if (!result) {
        events.push_back(degrade(result.error().message).front());
        return events;
    }

    const std::string final_text = strip_copy(result->text);
    if (!final_text.empty() && !turn_had_dialogue_) {
        const auto &npc = game_.world().npcs.at(npc_id);
        events.push_back({EventKind::dialogue, npc.identity.name + ": \"" + final_text + "\""});
    }
    return events;
}

nlohmann::json NpcSessionManager::snapshot_conversations() {
    nlohmann::json snapshot = nlohmann::json::object();
    for (auto &[npc_id, session] : sessions_) {
        auto doc = session->conversation.to_json();
        if (!doc) {
            continue;
        }
        auto parsed = nlohmann::json::parse(doc->text, nullptr, false);
        if (!parsed.is_discarded()) {
            snapshot[npc_id] = std::move(parsed);
        }
    }
    return snapshot;
}

void NpcSessionManager::restore_conversations(const nlohmann::json &conversations) {
    sessions_.clear();
    pending_restore_.clear();
    if (!conversations.is_object()) {
        return;
    }
    for (const auto &[npc_id, doc] : conversations.items()) {
        pending_restore_[npc_id] = doc;
    }
}

} // namespace chronicle
