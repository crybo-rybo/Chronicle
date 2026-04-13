/**
 * @file response_handler.cpp
 * @brief Implementation of @ref ResponseHandler — token forwarding and mutation narration.
 *
 * @details Implements the template-based mutation narration pipeline, including
 * the @c replace_all helper and the @c template_key dispatcher that maps
 * @ref MutationRequest::Type values to their string template keys.
 */

#include "ai/response_handler.hpp"
#include "entities/config.hpp"

namespace chronicle {

namespace {

std::string replace_all(std::string input, std::string_view needle, std::string_view value) {
    std::size_t pos = 0;
    while ((pos = input.find(needle, pos)) != std::string::npos) {
        input.replace(pos, needle.size(), value);
        pos += value.size();
    }
    return input;
}

std::string template_key(MutationRequest::Type type) {
    switch (type) {
    case MutationRequest::Type::GiveItemToPlayer:
        return "give_item_to_player";
    case MutationRequest::Type::TakeItemFromPlayer:
        return "take_item_from_player";
    case MutationRequest::Type::UpdateNpcMood:
        return "update_npc_mood";
    case MutationRequest::Type::UpdateNpcTrust:
        return "update_npc_trust";
    case MutationRequest::Type::MoveNpc:
        return "move_npc";
    case MutationRequest::Type::RevealKnowledge:
        return "reveal_knowledge";
    case MutationRequest::Type::AddMemory:
        return "add_memory";
    case MutationRequest::Type::SetFlag:
        return "set_flag";
    }
    return "";
}

} // namespace

ResponseHandler::ResponseHandler(Renderer &renderer, TokenQueue &token_queue, const World &world,
                                 std::unordered_map<std::string, std::string> templates)
    : renderer_(renderer), token_queue_(token_queue), world_(world),
      templates_(templates.empty() ? default_mutation_narration_templates() : std::move(templates)) {}

void ResponseHandler::on_token(std::string_view token) {
    token_queue_.push(std::string(token));
}

std::string ResponseHandler::describe_mutation(const MutationRequest &m,
                                               const std::string &npc_name) const {
    auto key = template_key(m.type);
    auto template_it = templates_.find(key);
    if (template_it == templates_.end() || template_it->second.empty()) {
        return "";
    }

    auto narration = template_it->second;
    narration = replace_all(std::move(narration), "{npc}", npc_name);

    if (auto item_it = m.params.find("item_id"); item_it != m.params.end()) {
        std::string item_name = item_it->second;
        if (auto world_it = world_.items.find(item_it->second); world_it != world_.items.end()) {
            item_name = world_it->second.name;
        }
        narration = replace_all(std::move(narration), "{item}", item_name);
    }

    if (auto mood_it = m.params.find("mood"); mood_it != m.params.end()) {
        narration = replace_all(std::move(narration), "{mood}", mood_it->second);
    }

    if (auto location_it = m.params.find("location_id"); location_it != m.params.end()) {
        std::string location_name = location_it->second;
        if (auto world_it = world_.locations.find(location_it->second);
            world_it != world_.locations.end()) {
            location_name = world_it->second.name;
        }
        narration = replace_all(std::move(narration), "{location}", location_name);
    }

    return narration;
}

void ResponseHandler::narrate_mutations(const std::vector<MutationRequest> &mutations,
                                        const std::string &npc_name) {
    for (const auto &m : mutations) {
        std::string narration = describe_mutation(m, npc_name);
        if (!narration.empty()) {
            renderer_.render_action(narration);
        }
    }
}

} // namespace chronicle
