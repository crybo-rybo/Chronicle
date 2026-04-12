#include "entities/location.hpp"
#include "entities/world.hpp"
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(LocationTest, DefaultConstruction) {
    Location loc;
    EXPECT_TRUE(loc.id.empty());
    EXPECT_TRUE(loc.name.empty());
    EXPECT_TRUE(loc.base_description.empty());
    EXPECT_TRUE(loc.current_description.empty());
    EXPECT_TRUE(loc.exits.empty());
    EXPECT_TRUE(loc.items.empty());
    EXPECT_TRUE(loc.npcs.empty());
    EXPECT_TRUE(loc.locked_exits.empty());
}

// ---------------------------------------------------------------------------
// describe() returns base_description when current_description is empty
// ---------------------------------------------------------------------------

TEST(LocationTest, DescribeReturnsBaseWhenCurrentEmpty) {
    Location loc;
    loc.base_description = "A dusty tavern with dim lighting.";

    World w;
    EXPECT_EQ(loc.describe(w), "A dusty tavern with dim lighting.");
}

// ---------------------------------------------------------------------------
// describe() returns current_description when set
// ---------------------------------------------------------------------------

TEST(LocationTest, DescribeReturnsCurrentWhenSet) {
    Location loc;
    loc.base_description = "A dusty tavern with dim lighting.";
    loc.current_description = "The tavern is bustling with activity tonight.";

    World w;
    EXPECT_EQ(loc.describe(w), "The tavern is bustling with activity tonight.");
}

// ---------------------------------------------------------------------------
// JSON roundtrip — all fields
// ---------------------------------------------------------------------------

TEST(LocationTest, JsonRoundtrip) {
    Location original;
    original.id = "tavern";
    original.name = "The Rusty Anchor";
    original.base_description = "A weathered tavern near the docks.";
    original.current_description = "The tavern is quiet this morning.";
    original.exits = {{"north", "market"}, {"east", "docks"}};
    original.items = {"tavern_key", "cargo_manifest"};
    original.npcs = {"marcus"};
    original.locked_exits = {"north"};

    nlohmann::json j;
    to_json(j, original);

    Location restored;
    from_json(j, restored);

    EXPECT_EQ(restored.id, original.id);
    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.base_description, original.base_description);
    EXPECT_EQ(restored.current_description, original.current_description);
    EXPECT_EQ(restored.exits, original.exits);
    EXPECT_EQ(restored.items, original.items);
    EXPECT_EQ(restored.npcs, original.npcs);
    EXPECT_EQ(restored.locked_exits, original.locked_exits);
}

// ---------------------------------------------------------------------------
// from_json: required fields — missing id or name throws
// ---------------------------------------------------------------------------

TEST(LocationTest, FromJsonRequiredFields) {
    // Missing id
    {
        nlohmann::json j = {{"name", "Some Place"}};
        Location loc;
        EXPECT_THROW(from_json(j, loc), nlohmann::json::exception);
    }
    // Missing name
    {
        nlohmann::json j = {{"id", "some_place"}};
        Location loc;
        EXPECT_THROW(from_json(j, loc), nlohmann::json::exception);
    }
}

// ---------------------------------------------------------------------------
// from_json: optional fields default correctly when absent
// ---------------------------------------------------------------------------

TEST(LocationTest, FromJsonOptionalDefaults) {
    nlohmann::json j = {{"id", "empty_room"}, {"name", "Empty Room"}};

    Location loc;
    from_json(j, loc);

    EXPECT_EQ(loc.id, "empty_room");
    EXPECT_EQ(loc.name, "Empty Room");
    EXPECT_TRUE(loc.base_description.empty());
    EXPECT_TRUE(loc.current_description.empty());
    EXPECT_TRUE(loc.exits.empty());
    EXPECT_TRUE(loc.items.empty());
    EXPECT_TRUE(loc.npcs.empty());
    EXPECT_TRUE(loc.locked_exits.empty());
}

} // namespace chronicle
