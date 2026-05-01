#include "entities/world_validator.hpp"
#include <gtest/gtest.h>

namespace chronicle {
namespace {

World make_valid_world() {
    World world;
    world.player.current_location = "tavern";

    Location tavern;
    tavern.id = "tavern";
    tavern.name = "Tavern";
    tavern.exits["east"] = "market";
    tavern.items = {"note"};
    tavern.npcs = {"marcus"};
    world.locations[tavern.id] = tavern;

    Location market;
    market.id = "market";
    market.name = "Market";
    market.exits["west"] = "tavern";
    world.locations[market.id] = market;

    Item note;
    note.id = "note";
    note.name = "Note";
    world.items[note.id] = note;

    Item reward;
    reward.id = "reward";
    reward.name = "Reward";
    world.items[reward.id] = reward;

    Npc marcus;
    marcus.identity.id = "marcus";
    marcus.identity.name = "Marcus";
    marcus.identity.knowledge = {"fact_known"};
    marcus.state.current_location = "tavern";
    marcus.state.mood = "neutral";
    world.npcs[marcus.identity.id] = marcus;

    world.facts["fact_known"] = Fact{"fact_known", "Marcus knows the truth.", "test", false};
    world.flags["intro_complete"] = false;

    EventTrigger event;
    event.id = "intro_event";
    event.conditions = {{ConditionType::PlayerAt, {"tavern"}}};
    event.actions = {{"narrate", {{"text", "The room settles."}}},
                     {"move_npc", {{"npc_id", "marcus"}, {"location_id", "market"}}},
                     {"set_flag", {{"flag_id", "intro_complete"}, {"value", "true"}}},
                     {"spawn_item", {{"item_id", "reward"}, {"location_id", "market"}}},
                     {"end_game", {}}};
    world.events.push_back(event);

    return world;
}

bool has_error_containing(const ValidationReport &report, std::string_view needle) {
    for (const auto &error : report.errors) {
        if (error.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(WorldValidatorTest, AcceptsValidWorldWithEventActions) {
    auto report = validate_world(make_valid_world());
    EXPECT_TRUE(report.ok);
    EXPECT_TRUE(report.errors.empty());
}

TEST(WorldValidatorTest, RejectsNpcKnowledgeWhenFactRegistryIsEmpty) {
    auto world = make_valid_world();
    world.facts.clear();

    auto report = validate_world(world);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "knowledge references fact 'fact_known'"));
}

TEST(WorldValidatorTest, RejectsInvalidEventConditions) {
    auto world = make_valid_world();
    world.events[0].conditions = {
        {ConditionType::ClockIs, {"later"}},
        {ConditionType::PlayerAt, {"missing_location"}},
        {ConditionType::FlagSet, {"missing_flag", "true"}},
        {ConditionType::NpcTrustGe, {"marcus", "not-an-int"}},
        {ConditionType::NpcAt, {"missing_npc", "tavern"}},
        {ConditionType::ItemInPlayerInv, {"missing_item"}},
        {ConditionType::TurnGe, {"-1"}},
    };

    auto report = validate_world(world);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "clock_is"));
    EXPECT_TRUE(has_error_containing(report, "missing_location"));
    EXPECT_TRUE(has_error_containing(report, "missing_flag"));
    EXPECT_TRUE(has_error_containing(report, "not-an-int"));
    EXPECT_TRUE(has_error_containing(report, "missing_npc"));
    EXPECT_TRUE(has_error_containing(report, "missing_item"));
    EXPECT_TRUE(has_error_containing(report, "non-negative integer"));
}

TEST(WorldValidatorTest, RejectsInvalidEventActions) {
    auto world = make_valid_world();
    world.events[0].actions = {
        {"teleport", {}},
        {"move_npc", {{"npc_id", "marcus"}, {"location_id", "missing_location"}}},
        {"set_flag", {{"flag_id", "intro_complete"}, {"value", "maybe"}}},
        {"spawn_item", {{"item_id", "missing_item"}, {"location_id", "market"}}},
        {"narrate", {{"text", ""}}},
    };

    auto report = validate_world(world);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "unknown action type 'teleport'"));
    EXPECT_TRUE(has_error_containing(report, "missing_location"));
    EXPECT_TRUE(has_error_containing(report, "must be a boolean"));
    EXPECT_TRUE(has_error_containing(report, "missing_item"));
    EXPECT_TRUE(has_error_containing(report, "non-empty 'text'"));
}

TEST(WorldValidatorTest, RejectsUnknownNpcToolPolicyTool) {
    auto world = make_valid_world();
    world.npcs.at("marcus").identity.tool_policy.allowed_tools = {"give_item", "invent_magic"};

    auto report = validate_world(world);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "unknown tool 'invent_magic'"));
}

TEST(WorldValidatorTest, RejectsDanglingNpcToolPolicyScopes) {
    auto world = make_valid_world();
    world.npcs.at("marcus").identity.tool_policy.allowed_items = {"missing_item"};
    world.npcs.at("marcus").identity.tool_policy.allowed_facts = {"missing_fact"};
    world.npcs.at("marcus").identity.tool_policy.allowed_flags = {"missing_flag"};
    world.npcs.at("marcus").identity.tool_policy.allowed_locations = {"missing_location"};

    auto report = validate_world(world);

    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(has_error_containing(report, "allowed_items references missing item"));
    EXPECT_TRUE(has_error_containing(report, "allowed_facts references missing fact"));
    EXPECT_TRUE(has_error_containing(report, "allowed_flags references missing flag"));
    EXPECT_TRUE(has_error_containing(report, "allowed_locations references missing location"));
}

} // namespace chronicle
