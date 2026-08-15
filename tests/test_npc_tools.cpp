// The action gate: validation and application of every NPC tool.
#include <gtest/gtest.h>

#include "chronicle/game/cartridge_game.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;

class GateTest : public ::testing::Test {
  protected:
    GateTest() : saves_("gatesaves"), game_(ct::make_test_world(), saves_.path()) {}

    [[nodiscard]] NpcData &keeper() { return game_.world().npcs.at("keeper"); }

    ct::TempDir saves_;
    CartridgeGame game_;
};

TEST_F(GateTest, SayEmitsDialogue) {
    const auto outcome = game_.submit_npc_tool("keeper", tools::Say{.text = "  Stay close.  "});
    ASSERT_TRUE(outcome.has_value());
    ASSERT_EQ(outcome->size(), 1u);
    EXPECT_EQ(outcome->front().kind, EventKind::dialogue);
    EXPECT_EQ(outcome->front().text, "Keeper: \"Stay close.\"");
}

TEST_F(GateTest, UnknownNpcRejected) {
    const auto outcome = game_.submit_npc_tool("ghost", tools::Say{.text = "boo"});
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().reason, "Unknown NPC");
}

TEST_F(GateTest, ToolNotInPolicyRejected) {
    keeper().identity.tool_policy.allowed_tools = {"say"};
    const auto outcome = game_.submit_npc_tool("keeper", tools::UpdateTrust{.delta = 5});
    ASSERT_FALSE(outcome.has_value());
    EXPECT_NE(outcome.error().reason.find("not allowed for Keeper"), std::string::npos);
}

TEST_F(GateTest, GiveItemTransfersToPlayer) {
    const auto outcome = game_.submit_npc_tool("keeper", tools::GiveItem{.item_id = "keepsake"});
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome->front().text, "Keeper hands you the Keepsake.");
    EXPECT_EQ(game_.world().item_owners.at("keepsake"), "player");
    EXPECT_TRUE(std::ranges::count(game_.world().player.inventory, std::string("keepsake")) == 1);
    EXPECT_TRUE(keeper().state.inventory.empty());
}

TEST_F(GateTest, GiveItemNotHeldRejected) {
    const auto outcome = game_.submit_npc_tool("keeper", tools::GiveItem{.item_id = "old_coin"});
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().reason, "NPC does not hold that item");
}

TEST_F(GateTest, GiveItemOutsideAllowedItemsRejected) {
    keeper().identity.tool_policy.allowed_items = {"old_coin"};
    const auto outcome = game_.submit_npc_tool("keeper", tools::GiveItem{.item_id = "keepsake"});
    ASSERT_FALSE(outcome.has_value());
    EXPECT_NE(outcome.error().reason.find("not allowed"), std::string::npos);
}

TEST_F(GateTest, TakeItemRequiresPlayerHolding) {
    const auto rejected = game_.submit_npc_tool("keeper", tools::TakeItem{.item_id = "old_coin"});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error().reason, "Player does not hold that item");

    (void)game_.handle_player("take old coin");
    const auto outcome = game_.submit_npc_tool("keeper", tools::TakeItem{.item_id = "old_coin"});
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome->front().text, "Keeper takes the Old Coin.");
    EXPECT_EQ(game_.world().item_owners.at("old_coin"), "keeper");
}

TEST_F(GateTest, UnknownItemRejected) {
    const auto outcome = game_.submit_npc_tool("keeper", tools::InspectItem{.item_id = "moon"});
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().reason, "Unknown item 'moon'");
}

TEST_F(GateTest, UpdateMoodAppliesAndNarrates) {
    const auto outcome =
        game_.submit_npc_tool("keeper", tools::UpdateMood{.mood = tools::Mood::hostile});
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(keeper().state.mood, "hostile");
    ASSERT_EQ(outcome->size(), 1u);
    EXPECT_NE(outcome->front().text.find("hostile"), std::string::npos);
}

TEST_F(GateTest, UpdateTrustClampsAndRevealsSecret) {
    (void)game_.submit_npc_tool("keeper", tools::UpdateTrust{.delta = -50});
    EXPECT_EQ(keeper().state.trust_toward_player, 0);
    EXPECT_FALSE(keeper().state.secret_revealed);

    (void)game_.submit_npc_tool("keeper", tools::UpdateTrust{.delta = 59});
    EXPECT_FALSE(keeper().state.secret_revealed);
    (void)game_.submit_npc_tool("keeper", tools::UpdateTrust{.delta = 1});
    EXPECT_EQ(keeper().state.trust_toward_player, 60);
    EXPECT_TRUE(keeper().state.secret_revealed);

    (void)game_.submit_npc_tool("keeper", tools::UpdateTrust{.delta = 999});
    EXPECT_EQ(keeper().state.trust_toward_player, 100);
}

TEST_F(GateTest, MoveSelfChecksPolicyAndEndsConversationWhenLeaving) {
    ASSERT_FALSE(
        game_.submit_npc_tool("keeper", tools::MoveSelf{.location_id = "void"}).has_value());
    keeper().identity.tool_policy.allowed_locations = {"hall"};
    const auto rejected = game_.submit_npc_tool("keeper", tools::MoveSelf{.location_id = "garden"});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error().reason, "Location not allowed");

    keeper().identity.tool_policy.allowed_locations = {"hall", "garden"};
    (void)game_.handle_player("talk keeper");
    ASSERT_EQ(game_.phase(), GamePhase::in_conversation);
    const auto outcome = game_.submit_npc_tool("keeper", tools::MoveSelf{.location_id = "garden"});
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(keeper().state.current_location, "garden");
    EXPECT_EQ(game_.phase(), GamePhase::playing);
    EXPECT_FALSE(game_.active_npc_id().has_value());
    // Default narration template for move_npc.
    ASSERT_EQ(outcome->size(), 1u);
    EXPECT_EQ(outcome->front().text, "Keeper excuses themselves and leaves.");
}

TEST_F(GateTest, RevealKnowledgeChecksAndReveals) {
    const auto unknown =
        game_.submit_npc_tool("keeper", tools::RevealKnowledge{.fact_id = "fact_moon"});
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().reason, "NPC does not know that fact");

    keeper().identity.tool_policy.allowed_facts = {"fact_other"};
    keeper().identity.knowledge.push_back("fact_other");
    const auto not_allowed =
        game_.submit_npc_tool("keeper", tools::RevealKnowledge{.fact_id = "fact_gate"});
    ASSERT_FALSE(not_allowed.has_value());
    EXPECT_EQ(not_allowed.error().reason, "Fact not allowed");

    keeper().identity.tool_policy.allowed_facts = {};
    const auto outcome =
        game_.submit_npc_tool("keeper", tools::RevealKnowledge{.fact_id = "fact_gate"});
    ASSERT_TRUE(outcome.has_value());
    EXPECT_TRUE(game_.world().revealed_facts.contains("fact_gate"));
    ASSERT_EQ(outcome->size(), 1u);
    EXPECT_EQ(outcome->front().kind, EventKind::knowledge);
    EXPECT_EQ(outcome->front().text, "The gate was locked after the accident.");
}

TEST_F(GateTest, RememberStoresMemoryWithPeriodTimestamp) {
    const auto rejected = game_.submit_npc_tool("keeper", tools::Remember{.summary = "   "});
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error().reason, "Memory summary required");

    const auto outcome = game_.submit_npc_tool(
        "keeper", tools::Remember{.summary = "The visitor asked about the gate.", .importance = 8});
    ASSERT_TRUE(outcome.has_value());
    ASSERT_EQ(keeper().state.memories.size(), 1u);
    const auto &memory = keeper().state.memories.front();
    EXPECT_EQ(memory.summary, "The visitor asked about the gate.");
    EXPECT_EQ(memory.importance, 8);
    EXPECT_EQ(memory.timestamp, "morning");
}

TEST_F(GateTest, SetFlagChecksPolicyAndApplies) {
    const auto unknown = game_.submit_npc_tool("keeper", tools::SetFlag{.flag_id = "no_flag"});
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().reason, "Unknown flag");

    const auto outcome =
        game_.submit_npc_tool("keeper", tools::SetFlag{.flag_id = "gate_seen", .value = true});
    ASSERT_TRUE(outcome.has_value());
    EXPECT_TRUE(game_.world().flags.at("gate_seen"));
}

TEST_F(GateTest, InspectItemReadsWithoutMutation) {
    const auto outcome = game_.submit_npc_tool("keeper", tools::InspectItem{.item_id = "old_coin"});
    ASSERT_TRUE(outcome.has_value());
    ASSERT_EQ(outcome->size(), 1u);
    EXPECT_EQ(outcome->front().kind, EventKind::tool_result);
    EXPECT_EQ(outcome->front().text, "[inspect] Old Coin: A worn coin.");
    EXPECT_EQ(game_.world().item_owners.at("old_coin"), "location");
}

TEST_F(GateTest, CustomNarrationTemplateWins) {
    game_.world().config.mutation_narration_templates["give_item_to_player"] =
        "{npc} presses the {item} into your hands.";
    const auto outcome = game_.submit_npc_tool("keeper", tools::GiveItem{.item_id = "keepsake"});
    ASSERT_TRUE(outcome.has_value());
    EXPECT_EQ(outcome->front().text, "Keeper presses the Keepsake into your hands.");
}

TEST_F(GateTest, EmptyNarrationTemplateSuppressesEvent) {
    game_.world().config.mutation_narration_templates["update_npc_mood"] = "";
    const auto outcome =
        game_.submit_npc_tool("keeper", tools::UpdateMood{.mood = tools::Mood::friendly});
    ASSERT_TRUE(outcome.has_value());
    EXPECT_TRUE(outcome->empty());
    EXPECT_EQ(keeper().state.mood, "friendly");
}

} // namespace
} // namespace chronicle
