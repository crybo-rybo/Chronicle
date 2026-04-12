#include "entities/world_loader.hpp"
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// Shared fixture load (used by most tests)
// ---------------------------------------------------------------------------

static World load_test_world() {
    return load_world(FIXTURES_DIR);
}

// ---------------------------------------------------------------------------
// Basic loading
// ---------------------------------------------------------------------------

TEST(WorldLoaderTest, LoadFromFixtures) {
    auto world = load_test_world();
    EXPECT_EQ(world.locations.size(), 1u);
    EXPECT_EQ(world.items.size(), 1u);
    EXPECT_EQ(world.npcs.size(), 1u);
    EXPECT_EQ(world.events.size(), 1u);
}

// ---------------------------------------------------------------------------
// Player start
// ---------------------------------------------------------------------------

TEST(WorldLoaderTest, PlayerStartLocation) {
    auto world = load_test_world();
    EXPECT_EQ(world.player.current_location, "test_room");
}

// ---------------------------------------------------------------------------
// ID injection from map keys
// ---------------------------------------------------------------------------

TEST(WorldLoaderTest, LocationIdSetFromKey) {
    auto world = load_test_world();
    EXPECT_EQ(world.locations.at("test_room").id, "test_room");
}

TEST(WorldLoaderTest, ItemIdSetFromKey) {
    auto world = load_test_world();
    EXPECT_EQ(world.items.at("test_item").id, "test_item");
}

TEST(WorldLoaderTest, NpcIdSetFromKey) {
    auto world = load_test_world();
    EXPECT_EQ(world.npcs.at("test_npc").identity.id, "test_npc");
}

TEST(WorldLoaderTest, EventIdSetFromKey) {
    auto world = load_test_world();
    EXPECT_EQ(world.events[0].id, "test_event");
}

// ---------------------------------------------------------------------------
// Cross-references
// ---------------------------------------------------------------------------

TEST(WorldLoaderTest, NpcAddedToLocationNpcs) {
    auto world = load_test_world();
    const auto &npcs = world.locations.at("test_room").npcs;
    EXPECT_NE(std::find(npcs.begin(), npcs.end(), "test_npc"), npcs.end());
}

// ---------------------------------------------------------------------------
// Clock defaults
// ---------------------------------------------------------------------------

TEST(WorldLoaderTest, ClockInitializedToDefaults) {
    auto world = load_test_world();
    EXPECT_EQ(world.clock.day, 1);
    EXPECT_EQ(world.clock.period, TimePeriod::Morning);
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

TEST(WorldLoaderTest, MissingFileThrows) {
    EXPECT_THROW(load_world("/nonexistent/path"), std::runtime_error);
}

} // namespace chronicle
