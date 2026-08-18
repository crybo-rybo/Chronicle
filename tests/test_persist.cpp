#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

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
