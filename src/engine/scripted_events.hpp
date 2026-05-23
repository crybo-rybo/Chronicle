/**
 * @file scripted_events.hpp
 * @brief Scripted event condition evaluation and action dispatch.
 */

#pragma once
#include "engine/mutation_request.hpp"
#include "entities/world.hpp"
#include <functional>
#include <string_view>

namespace chronicle {

struct ScriptedEventSink {
    std::function<void(MutationRequest)> enqueue_mutation;
    std::function<void(std::string_view)> narrate;
    std::function<void(std::string_view)> end_game;
};

struct ScriptedEventResult {
    bool ended_game = false;
};

/// @brief Evaluate eligible scripted events and dispatch actions through @p sink.
///
/// @details Mutates @p world event @c fired flags. Stops evaluating further events
/// when an @c end_game action fires.
ScriptedEventResult evaluate_scripted_events(World &world, ScriptedEventSink &sink);

} // namespace chronicle
