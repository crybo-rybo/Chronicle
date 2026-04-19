#include "engine/mutation_request.hpp"
#include "engine/mutations.hpp"
#include "entities/world.hpp"
#include <gtest/gtest.h>

using namespace chronicle;
using Source = MutationRequest::Source;
using Type = MutationRequest::Type;

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

        Item apple;
        apple.id = "apple";
        apple.name = "Apple";
        world.items["apple"] = apple;

        Item coin;
        coin.id = "coin";
        coin.name = "Coin";
        world.items["coin"] = coin;

        Item sword;
        sword.id = "sword";
        sword.name = "Sword";
        world.items["sword"] = sword;

        world.flags["door_open"] = false;

        Location tavern;
        tavern.id = "tavern";
        tavern.npcs = {"marcus"};
        tavern.items = {"coin"};
        world.locations["tavern"] = tavern;

        Location market;
        market.id = "market";
        world.locations["market"] = market;
    }
};

TEST_F(MutationsTest, ApplyGiveItemToPlayer) {
    MutationRequest req{Type::GiveItemToPlayer, Source::Npc, "marcus", {{"item_id", "apple"}}};
    EXPECT_TRUE(apply_mutation(world, req));

    EXPECT_TRUE(std::ranges::find(world.player.inventory, "apple") != world.player.inventory.end());
    EXPECT_TRUE(std::ranges::find(world.npcs["marcus"].state.inventory, "apple") == world.npcs["marcus"].state.inventory.end());
}

TEST_F(MutationsTest, ApplyTakeItemFromPlayer) {
    MutationRequest req{Type::TakeItemFromPlayer, Source::Npc, "marcus", {{"item_id", "sword"}}};
    EXPECT_TRUE(apply_mutation(world, req));

    EXPECT_TRUE(std::ranges::find(world.npcs["marcus"].state.inventory, "sword") != world.npcs["marcus"].state.inventory.end());
    EXPECT_TRUE(std::ranges::find(world.player.inventory, "sword") == world.player.inventory.end());
}

TEST_F(MutationsTest, ApplyUpdateNpcMood) {
    MutationRequest req{Type::UpdateNpcMood, Source::Npc, "marcus", {{"mood", "happy"}}};
    EXPECT_TRUE(apply_mutation(world, req));
    EXPECT_EQ(world.npcs["marcus"].state.mood, "happy");
}

TEST_F(MutationsTest, ApplyUpdateNpcTrust) {
    MutationRequest req{Type::UpdateNpcTrust, Source::Npc, "marcus", {{"delta", "10"}}};
    EXPECT_TRUE(apply_mutation(world, req));
    EXPECT_EQ(world.npcs["marcus"].state.trust_toward_player, 10);

    // Test clamping
    MutationRequest req2{Type::UpdateNpcTrust, Source::Npc, "marcus", {{"delta", "100"}}};
    EXPECT_TRUE(apply_mutation(world, req2));
    EXPECT_EQ(world.npcs["marcus"].state.trust_toward_player, 100);
}

TEST_F(MutationsTest, ApplyMoveNpc) {
    MutationRequest req{Type::MoveNpc, Source::Npc, "marcus", {{"location_id", "market"}}};
    EXPECT_TRUE(apply_mutation(world, req));

    EXPECT_EQ(world.npcs["marcus"].state.current_location, "market");

    auto& market_npcs = world.locations["market"].npcs;
    EXPECT_TRUE(std::ranges::find(market_npcs, "marcus") != market_npcs.end());

    auto& tavern_npcs = world.locations["tavern"].npcs;
    EXPECT_TRUE(std::ranges::find(tavern_npcs, "marcus") == tavern_npcs.end());
}

TEST_F(MutationsTest, ApplyRevealKnowledge) {
    MutationRequest req{Type::RevealKnowledge, Source::Npc, "marcus", {{"fact_id", "secret1"}}};
    EXPECT_TRUE(apply_mutation(world, req));

    EXPECT_TRUE(std::ranges::find(world.player.known_facts, "secret1") != world.player.known_facts.end());
}

TEST_F(MutationsTest, ApplyAddMemory) {
    MutationRequest req{Type::AddMemory, Source::Npc, "marcus", {{"summary", "Player was nice"}, {"importance", "5"}}};
    EXPECT_TRUE(apply_mutation(world, req));

    ASSERT_EQ(world.npcs["marcus"].state.memories.size(), 1);
    EXPECT_EQ(world.npcs["marcus"].state.memories[0].summary, "Player was nice");
    EXPECT_EQ(world.npcs["marcus"].state.memories[0].importance, 5);
}

TEST_F(MutationsTest, ApplySetFlag) {
    MutationRequest req{Type::SetFlag, Source::Npc, "", {{"flag_id", "door_open"}, {"value", "true"}}};
    EXPECT_TRUE(apply_mutation(world, req));

    EXPECT_TRUE(world.flags["door_open"]);
}

TEST_F(MutationsTest, MalformedMutationsReturnFalseWithoutThrowing) {
    std::vector<MutationRequest> bad_requests = {
        {Type::GiveItemToPlayer, Source::Npc, "marcus", {}},
        {Type::TakeItemFromPlayer, Source::Npc, "missing_npc", {{"item_id", "sword"}}},
        {Type::UpdateNpcMood, Source::Npc, "marcus", {}},
        {Type::UpdateNpcTrust, Source::Npc, "marcus", {{"delta", "not-an-int"}}},
        {Type::MoveNpc, Source::Npc, "marcus", {{"location_id", "missing_location"}}},
        {Type::RevealKnowledge, Source::Npc, "marcus", {}},
        {Type::AddMemory, Source::Npc, "marcus", {{"summary", "missing importance"}}},
        {Type::SetFlag, Source::Npc, "", {}},
    };

    for (const auto &req : bad_requests) {
        EXPECT_NO_THROW({ EXPECT_FALSE(apply_mutation(world, req)); });
    }
}

TEST_F(MutationsTest, AlreadyAppliedItemMutationsReturnFalseWithoutDuplicatingItems) {
    MutationRequest give{Type::GiveItemToPlayer, Source::Npc, "marcus", {{"item_id", "apple"}}};
    EXPECT_TRUE(apply_mutation(world, give));
    EXPECT_FALSE(apply_mutation(world, give));

    EXPECT_EQ(std::ranges::count(world.player.inventory, "apple"), 1);
    EXPECT_EQ(std::ranges::count(world.npcs["marcus"].state.inventory, "apple"), 0);
}

// ---------------------------------------------------------------------------
// Player mutation apply tests
// ---------------------------------------------------------------------------

TEST_F(MutationsTest, ApplyPlayerMove) {
    MutationRequest req{Type::PlayerMove, Source::Player, "player", {{"location_id", "market"}}};
    EXPECT_TRUE(apply_mutation(world, req));
    EXPECT_EQ(world.player.current_location, "market");
}

TEST_F(MutationsTest, ApplyPlayerMoveInvalidLocation) {
    MutationRequest req{Type::PlayerMove, Source::Player, "player", {{"location_id", "nowhere"}}};
    EXPECT_FALSE(apply_mutation(world, req));
    EXPECT_EQ(world.player.current_location, "tavern");
}

TEST_F(MutationsTest, ApplyPlayerTake) {
    MutationRequest req{Type::PlayerTake, Source::Player, "player", {{"item_id", "coin"}}};
    EXPECT_TRUE(apply_mutation(world, req));
    EXPECT_TRUE(std::ranges::contains(world.player.inventory, "coin"));
    EXPECT_FALSE(std::ranges::contains(world.locations["tavern"].items, "coin"));
}

TEST_F(MutationsTest, ApplyPlayerTakeNotInLocation) {
    MutationRequest req{Type::PlayerTake, Source::Player, "player", {{"item_id", "apple"}}};
    // apple is in NPC inventory, not in location
    EXPECT_FALSE(apply_mutation(world, req));
}

TEST_F(MutationsTest, ApplyPlayerDrop) {
    MutationRequest req{Type::PlayerDrop, Source::Player, "player", {{"item_id", "sword"}}};
    EXPECT_TRUE(apply_mutation(world, req));
    EXPECT_FALSE(std::ranges::contains(world.player.inventory, "sword"));
    EXPECT_TRUE(std::ranges::contains(world.locations["tavern"].items, "sword"));
}

TEST_F(MutationsTest, ApplyPlayerDropNotInInventory) {
    MutationRequest req{Type::PlayerDrop, Source::Player, "player", {{"item_id", "apple"}}};
    EXPECT_FALSE(apply_mutation(world, req));
}
