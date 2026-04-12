#include "engine/mutations.hpp"
#include "entities/world.hpp"
#include <gtest/gtest.h>

using namespace chronicle;

class MutationsTest : public ::testing::Test {
  protected:
    World world;

    void SetUp() override {
        Npc marcus;
        marcus.identity.id = "marcus";
        marcus.state.current_location = "tavern";
        marcus.state.inventory = {"apple", "coin"};
        marcus.state.trust_toward_player = 0;
        world.npcs["marcus"] = marcus;

        world.player.current_location = "tavern";
        world.player.inventory = {"sword"};

        Location tavern;
        tavern.id = "tavern";
        tavern.npcs = {"marcus"};
        world.locations["tavern"] = tavern;

        Location market;
        market.id = "market";
        world.locations["market"] = market;
    }
};

TEST_F(MutationsTest, ApplyGiveItemToPlayer) {
    MutationRequest req{MutationRequest::Type::GiveItemToPlayer, "marcus", {{"item_id", "apple"}}};
    apply_mutation(world, req);

    EXPECT_TRUE(std::ranges::find(world.player.inventory, "apple") != world.player.inventory.end());
    EXPECT_TRUE(std::ranges::find(world.npcs["marcus"].state.inventory, "apple") == world.npcs["marcus"].state.inventory.end());
}

TEST_F(MutationsTest, ApplyTakeItemFromPlayer) {
    MutationRequest req{MutationRequest::Type::TakeItemFromPlayer, "marcus", {{"item_id", "sword"}}};
    apply_mutation(world, req);

    EXPECT_TRUE(std::ranges::find(world.npcs["marcus"].state.inventory, "sword") != world.npcs["marcus"].state.inventory.end());
    EXPECT_TRUE(std::ranges::find(world.player.inventory, "sword") == world.player.inventory.end());
}

TEST_F(MutationsTest, ApplyUpdateNpcMood) {
    MutationRequest req{MutationRequest::Type::UpdateNpcMood, "marcus", {{"mood", "happy"}}};
    apply_mutation(world, req);
    EXPECT_EQ(world.npcs["marcus"].state.mood, "happy");
}

TEST_F(MutationsTest, ApplyUpdateNpcTrust) {
    MutationRequest req{MutationRequest::Type::UpdateNpcTrust, "marcus", {{"delta", "10"}}};
    apply_mutation(world, req);
    EXPECT_EQ(world.npcs["marcus"].state.trust_toward_player, 10);
    
    // Test clamping
    MutationRequest req2{MutationRequest::Type::UpdateNpcTrust, "marcus", {{"delta", "100"}}};
    apply_mutation(world, req2);
    EXPECT_EQ(world.npcs["marcus"].state.trust_toward_player, 100);
}

TEST_F(MutationsTest, ApplyMoveNpc) {
    MutationRequest req{MutationRequest::Type::MoveNpc, "marcus", {{"location_id", "market"}}};
    apply_mutation(world, req);

    EXPECT_EQ(world.npcs["marcus"].state.current_location, "market");
    
    auto& market_npcs = world.locations["market"].npcs;
    EXPECT_TRUE(std::ranges::find(market_npcs, "marcus") != market_npcs.end());
    
    auto& tavern_npcs = world.locations["tavern"].npcs;
    EXPECT_TRUE(std::ranges::find(tavern_npcs, "marcus") == tavern_npcs.end());
}

TEST_F(MutationsTest, ApplyRevealKnowledge) {
    MutationRequest req{MutationRequest::Type::RevealKnowledge, "marcus", {{"fact_id", "secret1"}}};
    apply_mutation(world, req);

    EXPECT_TRUE(std::ranges::find(world.player.known_facts, "secret1") != world.player.known_facts.end());
}

TEST_F(MutationsTest, ApplyAddMemory) {
    MutationRequest req{MutationRequest::Type::AddMemory, "marcus", {{"summary", "Player was nice"}, {"importance", "5"}}};
    apply_mutation(world, req);

    ASSERT_EQ(world.npcs["marcus"].state.memories.size(), 1);
    EXPECT_EQ(world.npcs["marcus"].state.memories[0].summary, "Player was nice");
    EXPECT_EQ(world.npcs["marcus"].state.memories[0].importance, 5);
}

TEST_F(MutationsTest, ApplySetFlag) {
    MutationRequest req{MutationRequest::Type::SetFlag, "marcus", {{"flag_id", "door_open"}, {"value", "true"}}};
    apply_mutation(world, req);

    EXPECT_TRUE(world.flags["door_open"]);
}
