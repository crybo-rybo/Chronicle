/**
 * @file response_handler.hpp
 * @brief Processes streaming LLM output and narrates applied mutations.
 *
 * @details @ref ResponseHandler sits between the LLM inference pipeline and
 * the renderer.  It has two distinct responsibilities:
 *
 * 1. **Token forwarding** — @ref on_token is called from the Zoo-Keeper
 *    inference thread for each generated token.  The token is pushed into the
 *    @ref TokenQueue so the main thread can drain it and pass it to the
 *    renderer for streaming display.
 *
 * 2. **Mutation narration** — @ref narrate_mutations is called on the main
 *    thread after @ref GameEngine::process_pending_mutations applies the
 *    queued state changes.  For each mutation, it resolves a narration
 *    template from the configurable template map, substitutes placeholders
 *    ({npc}, {item}, {mood}, {location}), and renders the result as an
 *    action narration.
 */

#pragma once
#include "ai/tool_registry.hpp"
#include "engine/token_queue.hpp"
#include "entities/world.hpp"
#include "rendering/renderer.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace chronicle {

/// @brief Bridges LLM inference output with the renderer and narration pipeline.
class ResponseHandler {
  public:
    /// @brief Construct with the required subsystem references.
    ///
    /// @details If @p templates is empty, @ref default_mutation_narration_templates
    /// is used to populate the narration template map.
    ///
    /// @param renderer    The renderer used to display action narration.
    /// @param token_queue The queue into which streaming tokens are pushed.
    /// @param world       Read-only world reference for resolving item and location names.
    /// @param templates   Optional custom narration templates keyed by mutation type string.
    ResponseHandler(Renderer &renderer, TokenQueue &token_queue, const World &world,
                    std::unordered_map<std::string, std::string> templates = {});

    /// @brief Push a single streaming token into the token queue.
    ///
    /// @details Called from the Zoo-Keeper inference thread.  Thread-safe via
    /// @ref TokenQueue::push.
    ///
    /// @param token The token text to enqueue.
    void on_token(std::string_view token);

    /// @brief Render narration for a list of applied mutations.
    ///
    /// @details Called on the main thread after mutations have been applied to
    /// the world.  For each mutation, resolves a narration template, substitutes
    /// placeholders, and calls @ref Renderer::render_action.  Mutations whose
    /// template is empty or absent produce no output.
    ///
    /// @param mutations  The mutations that were successfully applied.
    /// @param npc_name   The display name of the NPC that triggered the mutations,
    ///                   substituted for the @c {npc} placeholder.
    void narrate_mutations(const std::vector<MutationRequest> &mutations,
                           const std::string &npc_name);

    /// @brief Render narration for a single applied mutation.
    ///
    /// @details Avoids allocating a temporary vector for single-mutation narration.
    ///
    /// @param mutation  The mutation that was successfully applied.
    /// @param npc_name  The display name of the NPC that triggered the mutation.
    void narrate_mutation(const MutationRequest &mutation, const std::string &npc_name);

  private:
    Renderer &renderer_;                                          ///< Output renderer.
    TokenQueue &token_queue_;                                     ///< Streaming token buffer.
    const World &world_;                                          ///< Read-only world for name lookup.
    std::unordered_map<std::string, std::string> templates_;      ///< Mutation narration templates.

    /// @brief Resolve and populate a narration template for a single mutation.
    ///
    /// @details Looks up the template key corresponding to @c m.type, then
    /// replaces @c {npc}, @c {item}, @c {mood}, and @c {location} placeholders
    /// using data from @c m.params and the world.  Returns an empty string if
    /// no template is configured for this mutation type.
    ///
    /// @param m         The mutation to describe.
    /// @param npc_name  The display name to substitute for @c {npc}.
    /// @return The filled narration string, or an empty string to suppress narration.
    std::string describe_mutation(const MutationRequest &m, const std::string &npc_name) const;
};

} // namespace chronicle
