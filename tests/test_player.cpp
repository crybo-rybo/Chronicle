#include "entities/player.hpp"
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(PlayerTest, DefaultConstruction) {
    Player player;
    EXPECT_TRUE(player.current_location.empty());
    EXPECT_TRUE(player.inventory.empty());
    EXPECT_TRUE(player.known_facts.empty());
    EXPECT_TRUE(player.npc_encounter_count.empty());
    EXPECT_EQ(player.turns_in_conversation, 0);
}

// ---------------------------------------------------------------------------
// has_item
// ---------------------------------------------------------------------------

TEST(PlayerTest, HasItemTrue) {
    Player player;
    player.inventory.push_back("iron_key");
    EXPECT_TRUE(player.has_item("iron_key"));
}

TEST(PlayerTest, HasItemFalse) {
    Player player;
    EXPECT_FALSE(player.has_item("iron_key"));
}

// ---------------------------------------------------------------------------
// knows_fact
// ---------------------------------------------------------------------------

TEST(PlayerTest, KnowsFactTrue) {
    Player player;
    player.known_facts.push_back("blacksmith_name");
    EXPECT_TRUE(player.knows_fact("blacksmith_name"));
}

TEST(PlayerTest, KnowsFactFalse) {
    Player player;
    EXPECT_FALSE(player.knows_fact("blacksmith_name"));
}

// ---------------------------------------------------------------------------
// JSON roundtrip — all fields
// ---------------------------------------------------------------------------

TEST(PlayerTest, JsonRoundtrip) {
    Player original;
    original.current_location = "town_square";
    original.inventory = {"iron_key", "crumpled_note"};
    original.known_facts = {"blacksmith_name", "secret_passage"};
    original.npc_encounter_count = {{"guard", 3}, {"innkeeper", 1}};
    original.turns_in_conversation = 3;

    nlohmann::json j = original;
    Player restored = j.get<Player>();

    EXPECT_EQ(restored.current_location, original.current_location);
    EXPECT_EQ(restored.inventory, original.inventory);
    EXPECT_EQ(restored.known_facts, original.known_facts);
    EXPECT_EQ(restored.npc_encounter_count, original.npc_encounter_count);
    EXPECT_EQ(restored.turns_in_conversation, original.turns_in_conversation);
}

// ---------------------------------------------------------------------------
// NPC encounter count roundtrip
// ---------------------------------------------------------------------------

TEST(PlayerTest, NpcEncounterCountRoundtrip) {
    Player player;
    player.npc_encounter_count = {{"guard", 5}, {"merchant", 2}, {"wizard", 10}};

    nlohmann::json j = player;
    Player restored = j.get<Player>();

    ASSERT_EQ(restored.npc_encounter_count.size(), 3u);
    EXPECT_EQ(restored.npc_encounter_count.at("guard"), 5);
    EXPECT_EQ(restored.npc_encounter_count.at("merchant"), 2);
    EXPECT_EQ(restored.npc_encounter_count.at("wizard"), 10);
}

} // namespace chronicle
