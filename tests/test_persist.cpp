#include <fstream>
#include <gtest/gtest.h>

#include "chronicle/persist.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;

TEST(Persist, SaveLoadRoundTripPreservesWorld) {
    ct::TempDir dir("persist");
    SaveSystem saves(dir.path());
    WorldState world = ct::make_test_world();
    world.player.inventory.push_back("old_coin");
    world.flags["gate_seen"] = true;
    world.revealed_facts.insert("fact_gate");
    world.npcs.at("keeper").state.memories.push_back(
        {.timestamp = "morning",
         .type = "observation",
         .summary = "Met the visitor.",
         .importance = 5,
         .related_npc = "",
         .related_item = ""});
    world.clock.turns_elapsed = 3;

    const nlohmann::json conversations{{"keeper", {{"messages", nlohmann::json::array()}}}};
    saves.save(2, world, GamePhase::in_conversation, "keeper", conversations);

    const auto loaded = saves.load(2);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->phase, GamePhase::in_conversation);
    EXPECT_EQ(loaded->active_npc, "keeper");
    EXPECT_EQ(loaded->world.player.inventory, world.player.inventory);
    EXPECT_TRUE(loaded->world.flags.at("gate_seen"));
    EXPECT_TRUE(loaded->world.revealed_facts.contains("fact_gate"));
    EXPECT_EQ(loaded->world.npcs.at("keeper").state.memories.size(), 1u);
    EXPECT_EQ(loaded->world.clock.turns_elapsed, 3);
    EXPECT_TRUE(loaded->conversations.contains("keeper"));
}

TEST(Persist, NoActiveNpcSavesNull) {
    ct::TempDir dir("persistnull");
    SaveSystem saves(dir.path());
    saves.save(1, ct::make_test_world(), GamePhase::playing, std::nullopt);
    const auto loaded = saves.load(1);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->phase, GamePhase::playing);
    EXPECT_FALSE(loaded->active_npc.has_value());
}

TEST(Persist, MissingSlot) {
    ct::TempDir dir("persistmissing");
    const SaveSystem saves(dir.path());
    const auto loaded = saves.load(9);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::missing);
}

TEST(Persist, UnsupportedSchemaVersion) {
    ct::TempDir dir("persistschema");
    SaveSystem saves(dir.path());
    saves.save(1, ct::make_test_world(), GamePhase::playing, std::nullopt);
    // Corrupt the schema version in place.
    nlohmann::json data;
    {
        std::ifstream in(saves.path_for(1));
        in >> data;
    }
    data["save_schema_version"] = 99;
    {
        std::ofstream out(saves.path_for(1));
        out << data.dump();
    }
    const auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::unsupported_schema);
}

TEST(Persist, CorruptFile) {
    ct::TempDir dir("persistcorrupt");
    const SaveSystem saves(dir.path());
    std::ofstream out(saves.path_for(1));
    out << "{ nope";
    out.close();
    const auto loaded = saves.load(1);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().kind, SaveError::Kind::corrupt);
}

} // namespace
} // namespace chronicle
