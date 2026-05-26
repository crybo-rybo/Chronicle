#include "entities/world_loader.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// Shared fixture load (used by most tests)
// ---------------------------------------------------------------------------

static WorldFileSet fixture_file_set() {
    auto root = std::filesystem::path(FIXTURES_DIR);
    return WorldFileSet{.world = root / "world.json",
                        .npcs = root / "npcs.json",
                        .facts = root / "facts.json",
                        .flags = root / "flags.json",
                        .events = root / "events.json"};
}

static World load_test_world() {
    return load_world(fixture_file_set());
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

TEST(WorldLoaderTest, RevealedByDefaultAddsKnownFacts) {
    auto root = std::filesystem::path(FIXTURES_DIR);
    WorldFileSet files{.world = root / "world.json",
                       .npcs = root / "npcs.json",
                       .facts = root / "facts_revealed.json",
                       .flags = root / "flags.json",
                       .events = root / "events.json"};

    auto world = load_world(files);
    EXPECT_TRUE(world.player.knows_fact("fact_public"));
    EXPECT_FALSE(world.player.knows_fact("fact_test"));
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

TEST(WorldLoaderTest, MissingFileThrows) {
    auto files = fixture_file_set();
    files.world = std::filesystem::path(FIXTURES_DIR) / "missing_world.json";

    EXPECT_THROW(load_world(files), std::runtime_error);
}

TEST(WorldLoaderTest, MissingFactsFileThrows) {
    auto files = fixture_file_set();
    files.facts = std::filesystem::path(FIXTURES_DIR) / "missing_facts.json";

    EXPECT_THROW(load_world(files), std::runtime_error);
}

TEST(WorldLoaderTest, MissingFlagsFileThrows) {
    auto files = fixture_file_set();
    files.flags = std::filesystem::path(FIXTURES_DIR) / "missing_flags.json";

    EXPECT_THROW(load_world(files), std::runtime_error);
}

} // namespace chronicle
