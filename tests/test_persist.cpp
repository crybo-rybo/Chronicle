#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sys/stat.h>

#include "chronicle/json_text.hpp"
#include "chronicle/paths.hpp"
#include "chronicle/persist.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;
namespace fs = std::filesystem;
using nlohmann::json;

json read_json(const fs::path &path) {
    std::ifstream input(path);
    json document;
    input >> document;
    return document;
}

void write_json(const fs::path &path, const json &document) {
    std::ofstream output(path);
    output << document.dump(2);
}

TEST(Persist, SaveLoadRoundTripPreservesMutableRuntimeState) {
    ct::TempDir dir("persist");
    WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    world.item_positions["old_coin"] = ItemPosition{.holder = ItemHolder::player, .id = {}};
    world.flags["gate_seen"] = true;
    world.revealed_facts.insert("fact_gate");
    world.locations.at("hall").locked_exits.clear();
    world.events.at("evt_second_turn").fired = true;
    world.npcs.at("keeper").state.memories.push_back({.timestamp = "morning",
                                                      .type = "observation",
                                                      .summary = "Met the visitor.",
                                                      .importance = 5,
                                                      .related_npc = "",
                                                      .related_item = "old_coin"});
    world.clock.turns_elapsed = 3;

    const json conversations{{"keeper", {{"messages", json::array()}}}};
    ASSERT_TRUE(saves.save(2, world, GamePhase::in_conversation, "keeper", conversations));

    const auto loaded = saves.load(2);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    EXPECT_EQ(loaded->phase, GamePhase::in_conversation);
    EXPECT_EQ(loaded->active_npc, "keeper");
    EXPECT_EQ(loaded->world.item_positions, world.item_positions);
    EXPECT_TRUE(loaded->world.flags.at("gate_seen"));
    EXPECT_TRUE(loaded->world.revealed_facts.contains("fact_gate"));
    EXPECT_TRUE(loaded->world.locations.at("hall").locked_exits.empty());
    EXPECT_TRUE(loaded->world.events.at("evt_second_turn").fired);
    EXPECT_EQ(loaded->world.npcs.at("keeper").state.memories.size(), 1u);
    EXPECT_EQ(loaded->world.clock.turns_elapsed, 3);
    EXPECT_TRUE(loaded->conversations.contains("keeper"));
}

TEST(Persist, SaveContainsOnlyMutableWorldState) {
    ct::TempDir dir("persistmutable");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));

    const auto document = read_json(saves.path_for(1));
    EXPECT_FALSE(document.contains("world"));
    ASSERT_TRUE(document.contains("world_state"));
    EXPECT_FALSE(document["world_state"].contains("manifest"));
    EXPECT_FALSE(document["world_state"].contains("facts"));
    EXPECT_FALSE(document["world_state"].contains("events"));
    EXPECT_FALSE(document["world_state"].contains("npc_identities"));
}

TEST(Persist, CanonicalAuthoredDataCannotBeReplacedBySaveInput) {
    ct::TempDir dir("persistauthority");
    const WorldState canonical = ct::make_test_world();
    SaveSystem saves(dir.path(), canonical);
    WorldState altered = canonical;
    altered.facts.at("fact_gate").text = "Injected replacement fact";

    const auto saved = saves.save(1, altered, GamePhase::playing, std::nullopt);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().kind, SaveError::Kind::corrupt);
}

TEST(Persist, NoActiveNpcSavesNull) {
    ct::TempDir dir("persistnull");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    const auto loaded = saves.load(1);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->phase, GamePhase::playing);
    EXPECT_FALSE(loaded->active_npc.has_value());
}

TEST(Persist, MissingSlot) {
    ct::TempDir dir("persistmissing");
    const WorldState world = ct::make_test_world();
    const SaveSystem saves(dir.path(), world);
    const auto loaded = saves.load(9);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::missing);
}

TEST(Persist, InvalidSlotsAreRejectedWithoutCreatingPaths) {
    ct::TempDir dir("persistslot");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    for (const int slot : {0, -1, 100}) {
        const auto saved = saves.save(slot, world, GamePhase::playing, std::nullopt);
        ASSERT_FALSE(saved.has_value());
        EXPECT_EQ(saved.error().kind, SaveError::Kind::invalid_slot);
        const auto loaded = saves.load(slot);
        ASSERT_FALSE(loaded.has_value());
        EXPECT_EQ(loaded.error().kind, SaveError::Kind::invalid_slot);
        EXPECT_THROW((void)saves.path_for(slot), std::out_of_range);
    }
}

TEST(Persist, UnsupportedSchemaVersion) {
    ct::TempDir dir("persistschema");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    auto data = read_json(saves.path_for(1));
    data["save_schema_version"] = 99;
    write_json(saves.path_for(1), data);

    const auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::unsupported_schema);
}

TEST(Persist, WrongCartridgeIdentityIsRejected) {
    ct::TempDir dir("persistidentity");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    auto data = read_json(saves.path_for(1));
    data["cartridge_version"] = "9.9.9";
    write_json(saves.path_for(1), data);

    const auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::wrong_cartridge);
}

TEST(Persist, UnknownFieldsAndInvalidRuntimeStateAreRejected) {
    ct::TempDir dir("persiststrict");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));

    auto data = read_json(saves.path_for(1));
    data["world_state"]["npc_states"]["keeper"]["trust_toward_player"] = 101;
    write_json(saves.path_for(1), data);
    auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::corrupt);

    fs::remove(saves.path_for(1));
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    data = read_json(saves.path_for(1));
    data["world_state"]["authored_override"] = json::object();
    write_json(saves.path_for(1), data);
    loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::corrupt);
}

TEST(Persist, InvalidPhaseAndActiveNpcCombinationAreRejected) {
    ct::TempDir dir("persistphase");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));

    auto data = read_json(saves.path_for(1));
    data["phase"] = "future_phase";
    write_json(saves.path_for(1), data);
    auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::corrupt);

    fs::remove(saves.path_for(1));
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    data = read_json(saves.path_for(1));
    data["active_npc"] = "keeper";
    write_json(saves.path_for(1), data);
    loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::corrupt);
}

TEST(Persist, AtomicReplacementLeavesOneCompleteSave) {
    ct::TempDir dir("persistatomic");
    WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    world.flags["gate_seen"] = true;
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));

    const auto loaded = saves.load(1);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->world.flags.at("gate_seen"));
    std::size_t entries = 0;
    for (const auto &entry : fs::directory_iterator(dir.path())) {
        ++entries;
        EXPECT_EQ(entry.path().filename(), "slot_1.json");
    }
    EXPECT_EQ(entries, 1u);
}

TEST(Persist, OversizedAndSymlinkedSaveFilesAreRejected) {
    ct::TempDir dir("persistbounds");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);

    {
        std::ofstream output(saves.path_for(1), std::ios::binary);
        output.seekp(static_cast<std::streamoff>(MAX_SAVE_BYTES));
        output.put('\0');
    }
    auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::too_large);

    ASSERT_TRUE(saves.save(2, world, GamePhase::playing, std::nullopt));
    fs::remove(saves.path_for(1));
    fs::create_symlink(saves.path_for(2), saves.path_for(1));
    loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::io);
}

std::string nested(const std::size_t depth) {
    return std::string(depth, '[') + std::string(depth, ']');
}

TEST(Persist, DeeplyNestedSavesAreRefusedWithoutOverflow) {
    ct::TempDir dir("persistdeep");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    auto text = read_json(saves.path_for(1)).dump();
    const std::string empty = "\"conversations\":{}";
    text.replace(text.find(empty), empty.size(), "\"conversations\":" + nested(400'000));
    std::ofstream(saves.path_for(1)) << text;

    const auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::corrupt);
    EXPECT_NE(loaded.error().message.find("nested more than 512"), std::string::npos)
        << loaded.error().message;
}

TEST(Persist, DeepToolArgumentsInAConversationStillLoad) {
    ct::TempDir dir("persistargs");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    const json conversations{{"keeper", {{"arguments", json::parse(nested(251))}}}};
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt, conversations));
    const auto loaded = saves.load(1);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    EXPECT_EQ(loaded->conversations, conversations);
}

TEST(Persist, LockedExitsMustComeFromTheCartridge) {
    ct::TempDir dir("persistlocks");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    const auto original = read_json(saves.path_for(1));
    const json north{{"direction", "north"}, {"unlocked", false}};
    const json opened{{"direction", "north"}, {"unlocked", true}};
    const json south{{"direction", "south"}, {"unlocked", false}};
    const auto loads = [&](const std::string &location, const json &exits) {
        auto data = original;
        data["world_state"]["locked_exits"][location] = exits;
        write_json(saves.path_for(1), data);
        return saves.load(1).has_value();
    };
    EXPECT_TRUE(loads("hall", json::array({north})));
    EXPECT_TRUE(loads("hall", json::array()));
    EXPECT_FALSE(loads("hall", json::array({north, north})));
    EXPECT_FALSE(loads("hall", json::array({opened})));
    EXPECT_FALSE(loads("garden", json::array({south})));
}

TEST(Persist, SavesArePrivate) {
    ct::TempDir dir("persistmode");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    EXPECT_EQ(fs::status(saves.path_for(1)).permissions() &
                  (fs::perms::group_all | fs::perms::others_all),
              fs::perms::none);
}

TEST(JsonText, DepthIsCountedExactly) {
    EXPECT_NO_THROW((void)parse_untrusted_json(nested(64), 64));
    EXPECT_THROW((void)parse_untrusted_json(nested(65), 64), std::invalid_argument);
    const std::string objects = R"({"a":{"b":{"c":1}}})";
    EXPECT_NO_THROW((void)parse_untrusted_json(objects, 3));
    EXPECT_THROW((void)parse_untrusted_json(objects, 2), std::invalid_argument);
    // Siblings do not add up.
    EXPECT_NO_THROW((void)parse_untrusted_json("[[],[],[],{}]", 2));
}

TEST(Persist, ASaveOfAnotherCartridgeIsNotReplaced) {
    ct::TempDir dir("persistother");
    const WorldState world = ct::make_test_world();
    WorldState next_version = world;
    next_version.manifest.version = "2.0.0";
    WorldState namesake = world; // The same id and version, and a different world.
    namesake.locations.erase("garden");
    namesake.locations.at("hall").exits.clear();
    namesake.locations.at("hall").locked_exits.clear();
    namesake.npcs.at("keeper").identity.tool_policy.allowed_locations = {"hall"};

    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    const auto original = read_json(saves.path_for(1));
    for (const auto *other : {&next_version, &namesake}) {
        SaveSystem other_saves(dir.path(), *other);
        const auto refused = other_saves.save(1, *other, GamePhase::playing, std::nullopt);
        ASSERT_FALSE(refused.has_value());
        EXPECT_EQ(refused.error().kind, SaveError::Kind::occupied);
        EXPECT_EQ(read_json(saves.path_for(1)), original);
        ASSERT_TRUE(other_saves.save(2, *other, GamePhase::playing, std::nullopt));
        fs::remove(other_saves.path_for(2));
    }
    std::ofstream(saves.path_for(3)) << "{ damaged";
    EXPECT_EQ(saves.save(3, world, GamePhase::playing, std::nullopt).error().kind,
              SaveError::Kind::occupied);
    fs::create_symlink("nowhere.json", saves.path_for(4));
    EXPECT_EQ(saves.save(4, world, GamePhase::playing, std::nullopt).error().kind,
              SaveError::Kind::occupied);
    EXPECT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
}

TEST(Persist, SchemaVersionIsNotTruncated) {
    ct::TempDir dir("persistwide");
    const WorldState world = ct::make_test_world();
    SaveSystem saves(dir.path(), world);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    auto data = read_json(saves.path_for(1));
    data["save_schema_version"] = 4'294'967'297; // 1 in the low 32 bits.
    write_json(saves.path_for(1), data);
    const auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::unsupported_schema);
}

TEST(Persist, AFifoInASlotIsRefusedWithoutWaiting) {
    ct::TempDir dir("persistfifo");
    const WorldState world = ct::make_test_world();
    const SaveSystem saves(dir.path(), world);
    ASSERT_EQ(::mkfifo(saves.path_for(1).c_str(), 0600), 0);
    const auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::corrupt);
}

TEST(Persist, TheDirectoryIsCreatedByTheFirstSave) {
    ct::TempDir dir("persistlazy");
    const WorldState world = ct::make_test_world();
    const fs::path directory = dir.path() / "deep" / "saves";
    SaveSystem saves(directory, world);
    EXPECT_FALSE(fs::exists(directory));
    EXPECT_EQ(saves.load(1).error().kind, SaveError::Kind::missing);
    ASSERT_TRUE(saves.save(1, world, GamePhase::playing, std::nullopt));
    EXPECT_TRUE(saves.load(1).has_value());
}

TEST(Persist, AnUnusableDirectoryFailsTheSaveAndNothingElse) {
    ct::TempDir dir("persistunusable");
    const WorldState world = ct::make_test_world();
    std::ofstream(dir.path() / "file") << "in the way";
    SaveSystem blocked(dir.path() / "file" / "saves", world);
    const auto saved = blocked.save(1, world, GamePhase::playing, std::nullopt);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().kind, SaveError::Kind::io);

    fs::create_directory(dir.path() / "real");
    fs::create_directory_symlink(dir.path() / "real", dir.path() / "link");
    SaveSystem linked(dir.path() / "link", world);
    EXPECT_EQ(linked.save(1, world, GamePhase::playing, std::nullopt).error().kind,
              SaveError::Kind::io);
    EXPECT_EQ(linked.load(1).error().kind, SaveError::Kind::io);
    EXPECT_TRUE(fs::is_empty(dir.path() / "real"));
}

// --- where saves live ----------------------------------------------------------

class SaveDirectoryTest : public ::testing::Test {
  protected:
    SaveDirectoryTest()
        : home_("savehome"), work_("savework"), previous_directory_(fs::current_path()) {
        if (const char *home = std::getenv("HOME")) {
            previous_home_ = home;
        }
        (void)::setenv("HOME", home_.path().c_str(), 1);
        fs::current_path(work_.path());
    }
    ~SaveDirectoryTest() override {
        fs::current_path(previous_directory_);
        (void)::setenv("HOME", previous_home_.c_str(), 1);
    }

    ct::TempDir home_;
    ct::TempDir work_;
    fs::path previous_directory_;
    std::string previous_home_;
};

TEST_F(SaveDirectoryTest, DefaultsToTheHomeDirectoryAndCreatesNothing) {
    EXPECT_EQ(save_directory("case"), home_.path() / ".chronicle" / "saves" / "case");
    EXPECT_TRUE(fs::is_empty(home_.path()));
    EXPECT_TRUE(fs::is_empty(work_.path()));
}

TEST_F(SaveDirectoryTest, TheHostCanNameARoot) {
    EXPECT_EQ(save_directory("case", "/elsewhere"), fs::path("/elsewhere/case"));
}

TEST_F(SaveDirectoryTest, ACartridgeAlreadySavedInTheWorkingDirectoryStaysThere) {
    const auto legacy = work_.path() / "saves" / "case";
    fs::create_directories(legacy);
    EXPECT_EQ(save_directory("case"), home_.path() / ".chronicle" / "saves" / "case");
    std::ofstream(legacy / "slot_3.json") << "{}";
    EXPECT_TRUE(fs::equivalent(save_directory("case"), legacy));
    EXPECT_EQ(save_directory("other"), home_.path() / ".chronicle" / "saves" / "other");
}

TEST_F(SaveDirectoryTest, UnsafeIdsAreRefused) {
    EXPECT_THROW((void)save_directory("../escape"), std::invalid_argument);
    EXPECT_THROW((void)save_directory("../escape", "/elsewhere"), std::invalid_argument);
}

TEST(Persist, CorruptFile) {
    ct::TempDir dir("persistcorrupt");
    const WorldState world = ct::make_test_world();
    const SaveSystem saves(dir.path(), world);
    std::ofstream output(saves.path_for(1));
    output << "{ nope";
    output.close();
    const auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::corrupt);
}

} // namespace
} // namespace chronicle
