#include "engine/scripted_events.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace chronicle {
namespace {

World make_world_with_end_game_event() {
    World world;
    world.total_turns_elapsed = 1;

    EventTrigger event;
    event.id = "finale";
    event.once = true;
    event.conditions = {{ConditionType::TurnGe, {"1"}}};
    event.actions = {{"end_game", {}}};
    world.events.push_back(event);
    return world;
}

} // namespace

TEST(ScriptedEventsTest, EndGameActionInvokesSinkAndMarksOnceFired) {
    World world = make_world_with_end_game_event();
    std::vector<std::string> narrations;
    std::string ending;
    bool ended = false;

    ScriptedEventSink sink{
        [](MutationRequest) {},
        [&](std::string_view text) { narrations.push_back(std::string(text)); },
        [&](std::string_view text) {
            ending = std::string(text);
            ended = true;
        },
    };

    const auto result = evaluate_scripted_events(world, sink);

    EXPECT_TRUE(result.ended_game);
    EXPECT_TRUE(ended);
    EXPECT_EQ(ending, "The scenario has reached its conclusion.");
    EXPECT_TRUE(world.events[0].fired);
}

TEST(ScriptedEventsTest, NarrateActionDispatchesThroughSink) {
    World world;
    world.total_turns_elapsed = 1;

    EventTrigger event;
    event.id = "greeting";
    event.once = false;
    event.conditions = {{ConditionType::TurnGe, {"1"}}};
    event.actions = {{"narrate", {{"text", "Hello there."}}}};
    world.events.push_back(event);

    std::vector<std::string> narrations;
    ScriptedEventSink sink{
        [](MutationRequest) {},
        [&](std::string_view text) { narrations.push_back(std::string(text)); },
        [](std::string_view) {},
    };

    evaluate_scripted_events(world, sink);

    ASSERT_EQ(narrations.size(), 1u);
    EXPECT_EQ(narrations[0], "Hello there.");
}

} // namespace chronicle
