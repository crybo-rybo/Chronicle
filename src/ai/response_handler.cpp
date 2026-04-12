#include "ai/response_handler.hpp"
#include <sstream>

namespace chronicle {

ResponseHandler::ResponseHandler(Renderer &renderer, TokenQueue &token_queue, const World &world)
    : renderer_(renderer), token_queue_(token_queue), world_(world) {}

void ResponseHandler::on_token(std::string_view token) {
    token_queue_.push(std::string(token));
}

std::string ResponseHandler::describe_mutation(const MutationRequest &m,
                                               const std::string &npc_name) const {
    switch (m.type) {
    case MutationRequest::Type::GiveItemToPlayer: {
        auto it = m.params.find("item_id");
        if (it != m.params.end()) {
            std::string item_name = it->second;
            auto world_it = world_.items.find(it->second);
            if (world_it != world_.items.end()) {
                item_name = world_it->second.name;
            }
            return npc_name + " hands you the " + item_name + ".";
        }
        return npc_name + " hands you an item.";
    }
    case MutationRequest::Type::TakeItemFromPlayer: {
        auto it = m.params.find("item_id");
        if (it != m.params.end()) {
            std::string item_name = it->second;
            auto world_it = world_.items.find(it->second);
            if (world_it != world_.items.end()) {
                item_name = world_it->second.name;
            }
            return npc_name + " takes the " + item_name + ".";
        }
        return npc_name + " takes an item.";
    }
    case MutationRequest::Type::UpdateNpcMood: {
        auto it = m.params.find("mood");
        if (it != m.params.end()) {
            return npc_name + "'s expression shifts — they seem " + it->second + " now.";
        }
        break;
    }
    case MutationRequest::Type::MoveNpc: {
        return npc_name + " excuses themselves and leaves.";
    }
    case MutationRequest::Type::RevealKnowledge: {
        // Dialogue usually handles this, so action narration can be silent or subtle
        return "";
    }
    case MutationRequest::Type::UpdateNpcTrust:
    case MutationRequest::Type::AddMemory:
    case MutationRequest::Type::SetFlag:
        // Silent mutations
        return "";
    }
    return "";
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
