#include <gtest/gtest.h>

#include "chronicle/cartridge/loader.hpp"
#include "chronicle/cartridge/validator.hpp"
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

TEST(CartridgeLoad, DeeplyNestedJsonIsRefusedWithoutOverflow) {
    for (const std::string key : {"params", "ignored"}) {
        ct::TempDir dir("deepjson");
        ct::write_package(dir.path());
        std::ofstream(dir.path() / "events.json")
            << R"({"events":{"e":{"actions":[{"type":"narrate",")" << key << R"(":)"
            << std::string(200'000, '[') << std::string(200'000, ']') << "}]}}}";
        const auto issues = validate_package(dir.path());
        ASSERT_EQ(issues.size(), 1u);
        EXPECT_NE(issues.front().message.find("nested more than 64"), std::string::npos)
            << issues.front().message;
    }
}

TEST(CartridgeLoad, RepeatedKeysAreRefused) {
    ct::TempDir dir("repeatedkeys");
    ct::write_package(dir.path());
    std::ofstream(dir.path() / "flags.json")
        << R"({"flags":{"a":{"default":true},"b":{},"a":{"default":false}}})";
    const auto issues = validate_package(dir.path());
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_NE(issues.front().message.find("repeats the key \"a\""), std::string::npos)
        << issues.front().message;
}

TEST(CartridgeLoad, TheSameKeyInDifferentObjectsIsFine) {
    ct::TempDir dir("samekeys");
    ct::write_package(dir.path());
    std::ofstream(dir.path() / "flags.json")
        << R"({"flags":{"a":{"default":true},"b":{"default":false}}})";
    EXPECT_TRUE(load_package(dir.path()).flags.at("a"));
}

TEST(CartridgeLoad, EventParamsMustBeAnObject) {
    for (const std::string params : {R"("The end.")", "[]", "7"}) {
        ct::TempDir dir("stringparams");
        ct::write_package(dir.path());
        std::ofstream(dir.path() / "events.json")
            << R"({"events":{"e":{"actions":[{"type":"end_game","params":)" << params << "}]}}}";
        const auto issues = validate_package(dir.path());
        ASSERT_EQ(issues.size(), 1u) << params;
        EXPECT_NE(issues.front().message.find("end_game: params must be an object"),
                  std::string::npos)
            << issues.front().message;
    }
}

// The one problem validating a package finds, after overwriting one of its files.
std::string problem_with(const std::string &file, const std::string &text) {
    ct::TempDir dir("problem");
    ct::write_package(dir.path());
    std::ofstream(dir.path() / file) << text;
    const auto issues = validate_package(dir.path());
    EXPECT_EQ(issues.size(), 1u);
    return issues.empty() ? "" : issues.front().message;
}

TEST(CartridgeLoad, NumbersAreTakenAtTheirWord) {
    for (const std::string value : {"1.9", "4294967297", "true", "\"1\"", "-2147483649", "1e300"}) {
        EXPECT_EQ(
            problem_with("config.json", R"({"turns_per_period":)" + value + "}").substr(0, 53),
            "config.json: turns_per_period must be an integer, not")
            << value;
    }
    EXPECT_EQ(problem_with("config.json", R"({"temperature":true})"),
              "config.json: temperature must be a number, not true");

    ct::TempDir dir("wholenumbers");
    ct::write_package(dir.path());
    std::ofstream(dir.path() / "config.json") << R"({"turns_per_period":3.0,"temperature":1})";
    EXPECT_EQ(load_package(dir.path()).config.turns_per_period, 3);
}

TEST(CartridgeLoad, ConfigMustBeAnObject) {
    for (const std::string text : {"null", "[]", "7"}) {
        EXPECT_EQ(problem_with("config.json", text), "config.json: config.json must be an object");
    }
}

TEST(CartridgeLoad, ErrorsNameWhatTheyAreAbout) {
    EXPECT_EQ(problem_with("facts.json", R"({"facts":{"a":{"text":"x"},"b":{}}})"),
              "facts.json: fact b: key 'text' not found");
    EXPECT_EQ(problem_with("flags.json", R"({"flags":{"f":{"default":"yes"}}})"),
              "flags.json: flag f: type must be boolean, but is string");
    EXPECT_EQ(problem_with("npcs.json", R"({"npcs":{"n":{"identity":{"nam":"x"},"state":{}}}})"),
              "npcs.json: npc n: npc identity: unknown field 'nam'");
    EXPECT_EQ(
        problem_with("world.json", R"({"start_location":"a","locations":{"a":{"name":"A"}}})"),
        "world.json: location a: key 'base_description' not found");
    EXPECT_EQ(problem_with("events.json", R"({"events":[]})"),
              "events.json: events must be an object");
    EXPECT_NE(problem_with("flags.json", "{\n  nope").find("parse error at line 2"),
              std::string::npos);
}

TEST(CartridgeLoad, EveryFileMayNameItsSchema) {
    ct::TempDir dir("schemakey");
    ct::write_package(dir.path());
    for (const std::string file : {"scenario.json", "config.json", "world.json", "npcs.json",
                                   "facts.json", "flags.json", "events.json"}) {
        nlohmann::json document;
        std::ifstream(dir.path() / file) >> document;
        document["$schema"] = "https://chronicle.dev/schemas/" + file;
        std::ofstream(dir.path() / file) << document.dump();
    }
    EXPECT_TRUE(validate_package(dir.path()).empty());
}

TEST(CartridgeLoad, HiddenFilesAreNotPartOfAPackage) {
    ct::TempDir dir("hidden");
    ct::write_package(dir.path());
    std::filesystem::create_directories(dir.path() / ".git" / "objects");
    for (int index = 0; index < 300; ++index) {
        std::ofstream(dir.path() / ".git" / "objects" / std::to_string(index)) << "x";
    }
    std::error_code ec;
    std::filesystem::create_symlink("/etc", dir.path() / ".link", ec);
    EXPECT_EQ(inspect_package_tree(dir.path()).files.size(), 7u);
    EXPECT_TRUE(validate_package(dir.path()).empty());

    std::filesystem::rename(dir.path() / "world.json", dir.path() / ".world.json");
    ct::write_package(dir.path(), {{"scenario", {{"files", {{"world", ".world.json"}}}}}});
    const auto issues = validate_package(dir.path());
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_NE(issues.front().message.find("must not be hidden"), std::string::npos);
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

TEST(CartridgeLoad, LegacyConfigFieldsAreMappedOrIgnored) {
    using nlohmann::json;
    ct::TempDir dir("legacyconfig");
    ct::write_package(dir.path(),
                      {{"config",
                        {{"use_tui", true},
                         {"use_color", false},
                         {"mutation_narration_templates",
                          {{"give_item_to_player", "{npc} slides you the {item}."}}}}}});
    const WorldState world = load_package(dir.path());
    EXPECT_EQ(world.config.action_narration_templates.at("give_item"),
              "{npc} slides you the {item}.");
    const json dumped = world.config;
    EXPECT_FALSE(dumped.contains("use_tui"));
    EXPECT_FALSE(dumped.contains("use_color"));
    EXPECT_FALSE(dumped.contains("mutation_narration_templates"));
    EXPECT_EQ(dumped.at("action_narration_templates").at("give_item"),
              "{npc} slides you the {item}.");
}

TEST(CartridgeLoad, CanonicalNarrationTemplatesOverlayLegacyKeys) {
    ct::TempDir dir("narrationoverlay");
    ct::write_package(dir.path(),
                      {{"config",
                        {{"mutation_narration_templates", {{"give_item_to_player", "legacy"}}},
                         {"action_narration_templates", {{"give_item", "canonical"}}}}}});
    const WorldState world = load_package(dir.path());
    EXPECT_EQ(world.config.action_narration_templates.at("give_item"), "canonical");
}

TEST(CartridgeLoad, CanonicalNarrationKeyWinsInOneObjectToo) {
    ct::TempDir dir("narrationpair");
    ct::write_package(dir.path(),
                      {{"config",
                        {{"action_narration_templates",
                          {{"give_item", "canonical"}, {"give_item_to_player", "legacy"}}}}}});
    EXPECT_EQ(load_package(dir.path()).config.action_narration_templates.at("give_item"),
              "canonical");
}

TEST(CartridgeLoad, UnknownNarrationTemplateIsRejected) {
    ct::TempDir dir("badnarration");
    ct::write_package(dir.path(),
                      {{"config", {{"action_narration_templates", {{"explode", "boom"}}}}}});
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
