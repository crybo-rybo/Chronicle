#include <gtest/gtest.h>

#include "chronicle/cartridge/loader.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;

TEST(CartridgeLoad, MinimalExampleLoads) {
    const WorldState world = load_package(ct::minimal_example());
    EXPECT_EQ(world.manifest.id, "minimal");
    EXPECT_EQ(world.manifest.chronicle_schema_version, SCHEMA_VERSION);
    EXPECT_EQ(world.locations.size(), 2u);
    EXPECT_TRUE(world.npcs.contains("warden"));
    EXPECT_EQ(world.player.current_location, "foyer");
    EXPECT_TRUE(items_at(world, ItemHolder::player).empty());
    // Items get placed from location lists.
    EXPECT_TRUE(world.item_positions.at("visitor_ledger").is_location("foyer"));
}

TEST(CartridgeLoad, NpcIdentityIdDefaultsToMapKey) {
    const WorldState world = load_package(ct::minimal_example());
    EXPECT_EQ(world.npcs.at("warden").identity.id, "warden");
}

TEST(CartridgeLoad, NpcInventoryOwnsItems) {
    const WorldState world = ct::make_test_world();
    EXPECT_TRUE(world.item_positions.at("keepsake").is_npc("keeper"));
    EXPECT_EQ(world.npcs.at("keeper").state.current_location, "hall");
}

TEST(CartridgeLoad, ClockAdoptsConfig) {
    const WorldState world = ct::make_test_world();
    EXPECT_EQ(world.clock.turns_per_period, 2);
    EXPECT_EQ(world.clock.total_periods, 3);
}

TEST(CartridgeLoad, RevealedByDefaultFactsAreRevealed) {
    using nlohmann::json;
    ct::TempDir dir("revealed");
    ct::write_package(dir.path());
    // Rewrite facts with a default-revealed one.
    std::ofstream facts(dir.path() / "facts.json");
    facts << json{{"facts",
                   {{"open_fact", {{"text", "Known."}, {"revealed_by_default", true}}},
                    {"hidden_fact", {{"text", "Unknown."}}}}}}
                 .dump();
    facts.close();
    const WorldState world = load_package(dir.path());
    EXPECT_TRUE(world.revealed_facts.contains("open_fact"));
    EXPECT_FALSE(world.revealed_facts.contains("hidden_fact"));
}

TEST(CartridgeLoad, MissingDirectoryThrows) {
    EXPECT_THROW((void)load_package("/nonexistent/nowhere"), CartridgeError);
}

TEST(CartridgeLoad, MissingFileThrows) {
    ct::TempDir dir("missingfile");
    ct::write_package(dir.path());
    std::filesystem::remove(dir.path() / "npcs.json");
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, InvalidJsonThrows) {
    ct::TempDir dir("badjson");
    ct::write_package(dir.path());
    std::ofstream stream(dir.path() / "world.json");
    stream << "{ not json";
    stream.close();
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, PathTraversalRejected) {
    ct::TempDir dir("traversal");
    ct::write_package(dir.path(), {{"scenario", {{"files", {{"world", "../outside.json"}}}}}});
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, AbsolutePathRejected) {
    ct::TempDir dir("absolute");
    ct::write_package(dir.path(), {{"scenario", {{"files", {{"world", "/etc/hostname"}}}}}});
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, InvalidMoodRejectedAtParse) {
    using nlohmann::json;
    ct::TempDir dir("badmood");
    ct::write_package(dir.path());
    std::ofstream stream(dir.path() / "npcs.json");
    stream << json{{"npcs",
                    {{"ghost",
                      {{"identity", {{"name", "Ghost"}}},
                       {"state", {{"current_location", "cell"}, {"mood", "ecstatic"}}}}}}}}
                  .dump();
    stream.close();
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, UnknownFieldsAreRejected) {
    ct::TempDir dir("unknownfield");
    ct::write_package(dir.path(), {{"config", {{"mystery_option", true}}}});
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, CartridgeCannotConfigureNetworkOrCredentials) {
    ct::TempDir dir("networkauthority");
    ct::write_package(
        dir.path(),
        {{"config", {{"llm_base_url", "https://attacker.invalid/v1"}, {"llm_api_key", "stolen"}}}});
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, OversizedJsonFileIsRejected) {
    ct::TempDir dir("oversized");
    ct::write_package(dir.path());
    std::ofstream config(dir.path() / "config.json", std::ios::binary | std::ios::trunc);
    config << std::string(MAX_PACKAGE_FILE_BYTES + 1, 'x');
    config.close();
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, SymbolicLinksAreRejected) {
    ct::TempDir dir("symlink");
    ct::TempDir outside("symlinkoutside");
    ct::write_package(dir.path());
    std::ofstream(outside.path() / "data.json") << "{}";
    std::error_code ec;
    std::filesystem::create_symlink(outside.path() / "data.json", dir.path() / "extra.json", ec);
    if (ec) {
        GTEST_SKIP() << "symbolic links unavailable: " << ec.message();
    }
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, DuplicateInitialItemPlacementIsRejected) {
    using nlohmann::json;
    ct::TempDir dir("duplicateplacement");
    const json world{
        {"start_location", "cell"},
        {"locations",
         {{"cell",
           {{"name", "Cell"}, {"base_description", "A cell."}, {"items", json::array({"coin"})}}}}},
        {"items", {{"coin", {{"name", "Coin"}, {"description", "A coin."}}}}},
    };
    const json npcs{
        {"npcs",
         {{"guard",
           {{"identity", {{"name", "Guard"}}},
            {"state", {{"current_location", "cell"}, {"inventory", json::array({"coin"})}}}}}}},
    };
    ct::write_package(dir.path(), {{"world", world}, {"npcs", npcs}});
    EXPECT_THROW((void)load_package(dir.path()), CartridgeError);
}

TEST(CartridgeLoad, LockedExitsAcceptStringAndObjectForms) {
    using nlohmann::json;
    ct::TempDir dir("lockforms");
    const json world{
        {"start_location", "a"},
        {"locations",
         {{"a",
           {{"name", "A"},
            {"base_description", "room a"},
            {"exits", {{"north", "b"}, {"south", "b"}, {"east", "b"}}},
            {"locked_exits", json::array({"north",
                                          {{"direction", "south"}, {"unlocked", false}},
                                          {{"direction", "east"}, {"unlocked", true}}})}}},
          {"b", {{"name", "B"}, {"base_description", "room b"}}}}},
    };
    ct::write_package(dir.path(), {{"world", world}});
    const WorldState loaded = load_package(dir.path());
    const auto locked = loaded.locations.at("a").locked_directions();
    EXPECT_TRUE(locked.contains("north"));
    EXPECT_TRUE(locked.contains("south"));
    EXPECT_FALSE(locked.contains("east"));
}

TEST(CartridgeModels, ClockPeriodsWrapAndExpire) {
    ClockState clock{.turns_elapsed = 0, .turns_per_period = 2, .total_periods = 5};
    EXPECT_EQ(clock.period_name(), "morning");
    clock.turns_elapsed = 2;
    EXPECT_EQ(clock.period_name(), "afternoon");
    clock.turns_elapsed = 8; // period index 4 wraps to morning
    EXPECT_EQ(clock.period_name(), "morning");
    EXPECT_FALSE(clock.time_expired());
    clock.turns_elapsed = 10;
    EXPECT_TRUE(clock.time_expired());
}

TEST(CartridgeModels, WorldStateJsonRoundTrip) {
    const WorldState world = ct::make_test_world();
    const nlohmann::json encoded = world;
    const WorldState decoded = encoded.get<WorldState>();
    EXPECT_EQ(decoded.manifest.id, world.manifest.id);
    EXPECT_EQ(decoded.npcs.at("keeper").identity.secret, "The garden hides a grave.");
    EXPECT_EQ(decoded.item_positions, world.item_positions);
    EXPECT_EQ(decoded.flags, world.flags);
    EXPECT_EQ(decoded.events.at("evt_second_turn").actions.size(), 1u);
    const nlohmann::json reencoded = decoded;
    EXPECT_EQ(encoded, reencoded);
}

} // namespace
} // namespace chronicle
