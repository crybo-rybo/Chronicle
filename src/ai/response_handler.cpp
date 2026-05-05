/**
 * @file response_handler.cpp
 * @brief Implementation of @ref ResponseHandler — token forwarding and mutation narration.
 *
 * @details Implements the template-based mutation narration pipeline.  Template
 * keys are looked up via @ref mutation_type_name so the engine and response
 * handler share a single canonical mapping.
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

} // namespace

ResponseHandler::ResponseHandler(ActionNarrator action_narrator, TokenQueue &token_queue,
                                 const World &world,
                                 std::unordered_map<std::string, std::string> templates)
    : action_narrator_(std::move(action_narrator)), token_queue_(token_queue), world_(world),
      templates_(templates.empty() ? default_mutation_narration_templates()
                                   : std::move(templates)) {}

void ResponseHandler::on_token(std::string_view token) {
    token_queue_.push(std::string(token));
}

std::string ResponseHandler::describe_mutation(const MutationRequest &m,
                                               const std::string &npc_name) const {
    auto template_it = templates_.find(std::string(mutation_type_name(m.type)));
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
        narrate_mutation(m, npc_name);
    }
}

void ResponseHandler::narrate_mutation(const MutationRequest &mutation,
                                       const std::string &npc_name) {
    std::string narration = describe_mutation(mutation, npc_name);
    if (!narration.empty() && action_narrator_) {
        action_narrator_(narration);
    }
}

} // namespace chronicle
