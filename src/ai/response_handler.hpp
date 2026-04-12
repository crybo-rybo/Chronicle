#pragma once
#include "ai/tool_registry.hpp"
#include "engine/token_queue.hpp"
#include "entities/world.hpp"
#include "rendering/renderer.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace chronicle {

class ResponseHandler {
  public:
    ResponseHandler(Renderer &renderer, TokenQueue &token_queue, const World &world);

    // Called on inference thread
    void on_token(std::string_view token);

    // Called on main thread after inference completes
    void narrate_mutations(const std::vector<MutationRequest> &mutations,
                           const std::string &npc_name);

  private:
    Renderer &renderer_;
    TokenQueue &token_queue_;
    const World &world_;

    std::string describe_mutation(const MutationRequest &m, const std::string &npc_name) const;
};

} // namespace chronicle
