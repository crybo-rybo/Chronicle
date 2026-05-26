#include "engine/world_query.hpp"
#include <gtest/gtest.h>

namespace chronicle {
namespace {

World make_world() {
    World world;
    world.player.current_location = "tavern";

    Location tavern;
    tavern.id = "tavern";
    tavern.name = "Tavern";
    tavern.exits["north"] = "market";
    tavern.locked_exits = {"north"};
    tavern.items = {"note"};
    tavern.npcs = {"marcus"};
    world.locations["tavern"] = tavern;

    Location market;
    market.id = "market";
    market.name = "Market Square";
    world.locations["market"] = market;

    Item note;
    note.id = "note";
    note.name = "crumpled note";
    note.takeable = true;
    world.items["note"] = note;

    Item key;
    key.id = "brass_key";
    key.name = "brass key";
    key.unlock_target = "market";
    world.items["brass_key"] = key;

    Npc marcus;
    marcus.identity.id = "marcus";
    marcus.identity.name = "Marcus";
    marcus.state.current_location = "tavern";
    world.npcs["marcus"] = marcus;

    world.player.inventory = {"brass_key"};
    return world;
}

} // namespace

TEST(WorldQueryTest, FindVisibleNpcByName) {
    auto world = make_world();
    auto npc_id = find_visible_npc_id(world, "marc");
    ASSERT_TRUE(npc_id.has_value());
    EXPECT_EQ(*npc_id, "marcus");
}

TEST(WorldQueryTest, FindTakeableItemInLocation) {
    auto world = make_world();
    auto item_id = find_takeable_item_in_location(world, "crumpled");
    ASSERT_TRUE(item_id.has_value());
    EXPECT_EQ(*item_id, "note");
}

TEST(WorldQueryTest, FindLockedExitMatch) {
    auto world = make_world();
    auto match = find_locked_exit_match(world, "market");
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->direction, "north");
    EXPECT_EQ(match->destination_id, "market");
}

} // namespace chronicle
