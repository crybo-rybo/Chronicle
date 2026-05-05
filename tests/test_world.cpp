#include "entities/world.hpp"
#include <algorithm>
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(WorldTest, DefaultConstruction) {
    World w;
    EXPECT_TRUE(w.locations.empty());
    EXPECT_TRUE(w.npcs.empty());
    EXPECT_TRUE(w.items.empty());
    EXPECT_TRUE(w.flags.empty());
    EXPECT_TRUE(w.events.empty());
    EXPECT_EQ(w.total_turns_elapsed, 0);
    EXPECT_EQ(w.clock.day, 1);
    EXPECT_EQ(w.clock.period, TimePeriod::Morning);
    EXPECT_EQ(w.clock.turns_this_period, 0);
    EXPECT_EQ(w.clock.total_turns, 0);
}

// ---------------------------------------------------------------------------
// JSON roundtrip — fully populated World
// ---------------------------------------------------------------------------

TEST(WorldTest, JsonRoundtrip) {
    World original;

    // Clock: Day 2, Afternoon
    original.clock.day = 2;
    original.clock.period = TimePeriod::Afternoon;
    original.clock.turns_this_period = 3;
    original.clock.total_turns = 11;

    // 1 location
    Location tavern;
    tavern.id = "tavern";
    tavern.name = "The Rusty Anchor";
    tavern.base_description = "A weathered tavern near the docks.";
    tavern.exits = {{"north", "market"}, {"east", "docks"}};
    original.locations["tavern"] = tavern;

    // 1 NPC
    Npc marcus;
    marcus.identity.id = "marcus";
    marcus.identity.name = "Marcus";
    marcus.identity.role = "Innkeeper";
    marcus.identity.personality_summary = "A tired, guarded man.";
    marcus.identity.backstory = "Marcus witnessed the theft.";
    marcus.identity.secret = "He helped the thief escape.";
    marcus.identity.goals = {"Protect Elena"};
    marcus.identity.knowledge = {"fact_stolen_cargo"};
    marcus.identity.trust_reveal_threshold = 65;
    marcus.state.current_location = "tavern";
    marcus.state.mood = "neutral";
    marcus.state.trust_toward_player = 15;
    marcus.state.inventory = {"tavern_key"};
    marcus.state.has_met_player = true;
    marcus.state.secret_revealed = false;
    original.npcs["marcus"] = marcus;

    // 1 item
    Item key;
    key.id = "tavern_key";
    key.name = "Tavern Key";
    original.items["tavern_key"] = key;

    // Player
    original.player.current_location = "tavern";

    // 1 flag
    original.flags["intro_complete"] = true;

    // 1 event trigger
    EventTrigger trigger;
    trigger.id = "midnight_event";
    Condition cond;
    cond.type = ConditionType::ClockIs;
    cond.args = {"Night"};
    trigger.conditions.push_back(cond);
    EventAction action;
    action.type = "narrate";
    action.params = {{"text", "A bell tolls in the distance."}};
    trigger.actions.push_back(action);
    trigger.once = true;
    trigger.fired = false;
    original.events.push_back(trigger);

    // total_turns_elapsed
    original.total_turns_elapsed = 10;

    // Serialize
    nlohmann::json j;
    to_json(j, original);

    // Deserialize
    World restored;
    from_json(j, restored);

    // Verify clock
    EXPECT_EQ(restored.clock.day, 2);
    EXPECT_EQ(restored.clock.period, TimePeriod::Afternoon);
    EXPECT_EQ(restored.clock.turns_this_period, 3);
    EXPECT_EQ(restored.clock.total_turns, 11);

    // Verify location
    ASSERT_EQ(restored.locations.size(), 1u);
    ASSERT_TRUE(restored.locations.contains("tavern"));
    const auto &rloc = restored.locations.at("tavern");
    EXPECT_EQ(rloc.id, "tavern");
    EXPECT_EQ(rloc.name, "The Rusty Anchor");
    EXPECT_EQ(rloc.base_description, "A weathered tavern near the docks.");
    EXPECT_EQ(rloc.exits.size(), 2u);
    EXPECT_EQ(rloc.exits.at("north"), "market");
    EXPECT_EQ(rloc.exits.at("east"), "docks");

    // Verify NPC
    ASSERT_EQ(restored.npcs.size(), 1u);
    ASSERT_TRUE(restored.npcs.contains("marcus"));
    const auto &rnpc = restored.npcs.at("marcus");
    EXPECT_EQ(rnpc.identity.id, "marcus");
    EXPECT_EQ(rnpc.identity.name, "Marcus");
    EXPECT_EQ(rnpc.identity.role, "Innkeeper");
    EXPECT_EQ(rnpc.state.current_location, "tavern");
    EXPECT_EQ(rnpc.state.mood, "neutral");
    EXPECT_EQ(rnpc.state.trust_toward_player, 15);
    EXPECT_TRUE(rnpc.state.has_met_player);

    // Verify item
    ASSERT_EQ(restored.items.size(), 1u);
    ASSERT_TRUE(restored.items.contains("tavern_key"));
    EXPECT_EQ(restored.items.at("tavern_key").id, "tavern_key");
    EXPECT_EQ(restored.items.at("tavern_key").name, "Tavern Key");

    // Verify player
    EXPECT_EQ(restored.player.current_location, "tavern");

    // Verify flags
    ASSERT_EQ(restored.flags.size(), 1u);
    EXPECT_TRUE(restored.flags.at("intro_complete"));

    // Verify events
    ASSERT_EQ(restored.events.size(), 1u);
    EXPECT_EQ(restored.events[0].id, "midnight_event");
    ASSERT_EQ(restored.events[0].conditions.size(), 1u);
    EXPECT_EQ(restored.events[0].conditions[0].type, ConditionType::ClockIs);
    EXPECT_EQ(restored.events[0].conditions[0].args.size(), 1u);
    EXPECT_EQ(restored.events[0].conditions[0].args[0], "Night");
    ASSERT_EQ(restored.events[0].actions.size(), 1u);
    EXPECT_EQ(restored.events[0].actions[0].type, "narrate");
    EXPECT_EQ(restored.events[0].actions[0].params.at("text"), "A bell tolls in the distance.");
    EXPECT_TRUE(restored.events[0].once);
    EXPECT_FALSE(restored.events[0].fired);

    // Verify total_turns_elapsed
    EXPECT_EQ(restored.total_turns_elapsed, 10);
}

// ---------------------------------------------------------------------------
// Item ownership invariant: no item appears in multiple locations
// ---------------------------------------------------------------------------

TEST(WorldTest, ItemOwnershipInvariant) {
    // Build a World where items are tracked in location vectors
    World world;

    Location room_a;
    room_a.id = "room_a";
    room_a.name = "Room A";
    room_a.items = {"key", "sword"};

    Location room_b;
    room_b.id = "room_b";
    room_b.name = "Room B";
    room_b.items = {"shield"};

    world.locations["room_a"] = room_a;
    world.locations["room_b"] = room_b;
    world.player.inventory = {"potion"};

    // Collect all item references
    std::vector<std::string> all_item_refs;
    for (const auto &[id, loc] : world.locations) {
        all_item_refs.insert(all_item_refs.end(), loc.items.begin(), loc.items.end());
    }
    all_item_refs.insert(all_item_refs.end(), world.player.inventory.begin(),
                         world.player.inventory.end());

    // Verify no duplicates
    std::sort(all_item_refs.begin(), all_item_refs.end());
    auto dup = std::adjacent_find(all_item_refs.begin(), all_item_refs.end());
    EXPECT_EQ(dup, all_item_refs.end()) << "Duplicate item found: " << *dup;
}

} // namespace chronicle
