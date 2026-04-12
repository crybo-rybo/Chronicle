#include "entities/npc.hpp"
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// NpcIdentity defaults
// ---------------------------------------------------------------------------

TEST(NpcTest, NpcIdentityDefaults) {
    NpcIdentity identity;
    EXPECT_TRUE(identity.id.empty());
    EXPECT_TRUE(identity.name.empty());
    EXPECT_TRUE(identity.role.empty());
    EXPECT_TRUE(identity.personality_summary.empty());
    EXPECT_TRUE(identity.backstory.empty());
    EXPECT_TRUE(identity.secret.empty());
    EXPECT_TRUE(identity.goals.empty());
    EXPECT_TRUE(identity.knowledge.empty());
    EXPECT_EQ(identity.trust_reveal_threshold, 70);
}

// ---------------------------------------------------------------------------
// NpcState defaults
// ---------------------------------------------------------------------------

TEST(NpcTest, NpcStateDefaults) {
    NpcState state;
    EXPECT_TRUE(state.current_location.empty());
    EXPECT_TRUE(state.mood.empty());
    EXPECT_EQ(state.trust_toward_player, 0);
    EXPECT_TRUE(state.inventory.empty());
    EXPECT_TRUE(state.memories.empty());
    EXPECT_FALSE(state.has_met_player);
    EXPECT_FALSE(state.secret_revealed);
}

// ---------------------------------------------------------------------------
// Full NPC JSON roundtrip
// ---------------------------------------------------------------------------

TEST(NpcTest, NpcJsonRoundtrip) {
    Npc original;
    original.identity.id = "marcus";
    original.identity.name = "Marcus";
    original.identity.role = "Innkeeper";
    original.identity.personality_summary = "A tired, guarded man.";
    original.identity.backstory = "Marcus witnessed the theft.";
    original.identity.secret = "He helped the thief escape.";
    original.identity.goals = {"Protect Elena", "Keep the tavern running"};
    original.identity.knowledge = {"fact_stolen_cargo", "fact_thief_identity"};
    original.identity.trust_reveal_threshold = 65;

    original.state.current_location = "tavern";
    original.state.mood = "neutral";
    original.state.trust_toward_player = 15;
    original.state.inventory = {"tavern_key", "cargo_manifest"};
    original.state.has_met_player = true;
    original.state.secret_revealed = false;

    MemoryEntry mem;
    mem.timestamp = "Morning, Day 1";
    mem.type = "conversation";
    mem.summary = "The player asked about the stolen cargo.";
    mem.importance = 5;
    mem.related_npc = "player";
    mem.related_item = "cargo_manifest";
    original.state.memories.push_back(mem);

    nlohmann::json j = original;
    Npc restored = j.get<Npc>();

    // Identity fields
    EXPECT_EQ(restored.identity.id, original.identity.id);
    EXPECT_EQ(restored.identity.name, original.identity.name);
    EXPECT_EQ(restored.identity.role, original.identity.role);
    EXPECT_EQ(restored.identity.personality_summary, original.identity.personality_summary);
    EXPECT_EQ(restored.identity.backstory, original.identity.backstory);
    EXPECT_EQ(restored.identity.secret, original.identity.secret);
    EXPECT_EQ(restored.identity.goals, original.identity.goals);
    EXPECT_EQ(restored.identity.knowledge, original.identity.knowledge);
    EXPECT_EQ(restored.identity.trust_reveal_threshold, original.identity.trust_reveal_threshold);

    // State fields
    EXPECT_EQ(restored.state.current_location, original.state.current_location);
    EXPECT_EQ(restored.state.mood, original.state.mood);
    EXPECT_EQ(restored.state.trust_toward_player, original.state.trust_toward_player);
    EXPECT_EQ(restored.state.inventory, original.state.inventory);
    EXPECT_EQ(restored.state.has_met_player, original.state.has_met_player);
    EXPECT_EQ(restored.state.secret_revealed, original.state.secret_revealed);

    // Memory roundtrip
    ASSERT_EQ(restored.state.memories.size(), 1u);
    EXPECT_EQ(restored.state.memories[0].timestamp, mem.timestamp);
    EXPECT_EQ(restored.state.memories[0].type, mem.type);
    EXPECT_EQ(restored.state.memories[0].summary, mem.summary);
    EXPECT_EQ(restored.state.memories[0].importance, mem.importance);
    EXPECT_EQ(restored.state.memories[0].related_npc, mem.related_npc);
    EXPECT_EQ(restored.state.memories[0].related_item, mem.related_item);
}

// ---------------------------------------------------------------------------
// Nested JSON structure
// ---------------------------------------------------------------------------

TEST(NpcTest, NpcNestedJsonStructure) {
    Npc npc;
    npc.identity.id = "test";
    npc.identity.name = "Test NPC";
    npc.state.current_location = "market";

    nlohmann::json j = npc;

    EXPECT_TRUE(j.contains("identity"));
    EXPECT_TRUE(j.contains("state"));
    EXPECT_EQ(j["identity"]["id"], "test");
    EXPECT_EQ(j["identity"]["name"], "Test NPC");
    EXPECT_EQ(j["state"]["current_location"], "market");
}

// ---------------------------------------------------------------------------
// Memory in state roundtrip
// ---------------------------------------------------------------------------

TEST(NpcTest, NpcMemoryInState) {
    Npc npc;
    npc.identity.id = "elena";

    MemoryEntry mem1;
    mem1.timestamp = "Morning, Day 1";
    mem1.type = "conversation";
    mem1.summary = "Player introduced themselves.";
    mem1.importance = 3;
    mem1.related_npc = "player";
    mem1.related_item = "";

    MemoryEntry mem2;
    mem2.timestamp = "Afternoon, Day 1";
    mem2.type = "observation";
    mem2.summary = "Player was seen near the docks.";
    mem2.importance = 7;
    mem2.related_npc = "";
    mem2.related_item = "stolen_crate";

    npc.state.memories = {mem1, mem2};

    nlohmann::json j = npc;
    Npc restored = j.get<Npc>();

    ASSERT_EQ(restored.state.memories.size(), 2u);

    EXPECT_EQ(restored.state.memories[0].timestamp, "Morning, Day 1");
    EXPECT_EQ(restored.state.memories[0].type, "conversation");
    EXPECT_EQ(restored.state.memories[0].summary, "Player introduced themselves.");
    EXPECT_EQ(restored.state.memories[0].importance, 3);
    EXPECT_EQ(restored.state.memories[0].related_npc, "player");
    EXPECT_EQ(restored.state.memories[0].related_item, "");

    EXPECT_EQ(restored.state.memories[1].timestamp, "Afternoon, Day 1");
    EXPECT_EQ(restored.state.memories[1].type, "observation");
    EXPECT_EQ(restored.state.memories[1].summary, "Player was seen near the docks.");
    EXPECT_EQ(restored.state.memories[1].importance, 7);
    EXPECT_EQ(restored.state.memories[1].related_npc, "");
    EXPECT_EQ(restored.state.memories[1].related_item, "stolen_crate");
}

// ---------------------------------------------------------------------------
// NPC convenience: trust_level() delegates to state
// ---------------------------------------------------------------------------

TEST(NpcTest, TrustLevelDelegatesToState) {
    Npc npc;
    npc.state.trust_toward_player = 42;
    EXPECT_EQ(npc.trust_level(), 42);
}

// ---------------------------------------------------------------------------
// NPC convenience: is_hostile() checks mood
// ---------------------------------------------------------------------------

TEST(NpcTest, IsHostileWhenMoodHostile) {
    Npc npc;
    npc.state.mood = "hostile";
    EXPECT_TRUE(npc.is_hostile());
}

TEST(NpcTest, IsNotHostileWhenMoodNeutral) {
    Npc npc;
    npc.state.mood = "neutral";
    EXPECT_FALSE(npc.is_hostile());
}

// ---------------------------------------------------------------------------
// NpcState convenience: has_item()
// ---------------------------------------------------------------------------

TEST(NpcTest, StateHasItemTrue) {
    NpcState state;
    state.inventory = {"sword", "shield"};
    EXPECT_TRUE(state.has_item("sword"));
}

TEST(NpcTest, StateHasItemFalse) {
    NpcState state;
    state.inventory = {"sword"};
    EXPECT_FALSE(state.has_item("shield"));
}

// ---------------------------------------------------------------------------
// NpcState convenience: add_memory()
// ---------------------------------------------------------------------------

TEST(NpcTest, AddMemoryAppendsToMemories) {
    NpcState state;
    MemoryEntry entry;
    entry.timestamp = "Morning, Day 1";
    entry.type = "conversation";
    entry.summary = "Met the player.";
    entry.importance = 5;

    state.add_memory(std::move(entry));

    ASSERT_EQ(state.memories.size(), 1u);
    EXPECT_EQ(state.memories[0].summary, "Met the player.");
    EXPECT_EQ(state.memories[0].importance, 5);
}

} // namespace chronicle
