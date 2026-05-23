#include "ai/tool_registry.hpp"
#include "engine/mutations.hpp"
#include <gtest/gtest.h>

namespace chronicle {

// ---------------------------------------------------------------------------
// Fixture: builds a small world for ToolRegistry validation tests
// ---------------------------------------------------------------------------

class ToolRegistryTest : public ::testing::Test {
  protected:
    World world;

    void SetUp() override {
        // NPC marcus at tavern with inventory [tavern_key, cargo_manifest]
        Npc marcus;
        marcus.identity.id = "marcus";
        marcus.identity.name = "Marcus";
        marcus.identity.knowledge = {"fact_stolen_cargo", "fact_thief_identity"};
        marcus.state.current_location = "tavern";
        marcus.state.inventory = {"tavern_key", "cargo_manifest"};
        world.npcs["marcus"] = marcus;

        // Items
        Item key;
        key.id = "tavern_key";
        key.name = "tavern key";
        key.key_item = true;
        world.items["tavern_key"] = key;

        Item manifest;
        manifest.id = "cargo_manifest";
        manifest.name = "Cargo Manifest";
        manifest.description = "A detailed list of goods.";
        manifest.key_item = false;
        manifest.properties = {{"readable", "true"},
                               {"text", "Shipment 47: 12 bolts silk, 1 sealed chest."}};
        world.items["cargo_manifest"] = manifest;

        Item note;
        note.id = "crumpled_note";
        note.name = "crumpled note";
        note.key_item = false;
        note.properties = {{"readable", "true"}, {"text", "Don't trust the innkeeper."}};
        world.items["crumpled_note"] = note;

        // Player at tavern with crumpled_note
        world.player.current_location = "tavern";
        world.player.inventory = {"crumpled_note"};

        // Locations
        Location tavern;
        tavern.id = "tavern";
        tavern.name = "Tavern";
        world.locations["tavern"] = tavern;

        Location market;
        market.id = "market_square";
        market.name = "Market Square";
        world.locations["market_square"] = market;

        // Flags
        world.flags["intro_complete"] = true;
    }
};

// ---------------------------------------------------------------------------
// GiveItem tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, GiveItemRejected_NpcLacksItem) {
    ToolRegistry reg(world);
    auto result = reg.validate_give_item("marcus", "crumpled_note");
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

TEST_F(ToolRegistryTest, GiveItemAccepted_NpcHasItem) {
    ToolRegistry reg(world);
    auto result = reg.validate_give_item("marcus", "cargo_manifest");
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::GiveItemToPlayer);
    EXPECT_EQ(req.actor_id, "marcus");
    EXPECT_EQ(req.params.at("item_id"), "cargo_manifest");
}

TEST_F(ToolRegistryTest, GiveItemRejected_ItemNotExist) {
    ToolRegistry reg(world);
    auto result = reg.validate_give_item("marcus", "nonexistent");
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

// ---------------------------------------------------------------------------
// TakeItem tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, TakeItemRejected_KeyItem) {
    // Give the player a key item so we can test the key_item rejection path
    Item special;
    special.id = "special_key";
    special.name = "special key";
    special.key_item = true;
    world.items["special_key"] = special;
    world.player.inventory.push_back("special_key");

    ToolRegistry reg(world);
    auto result = reg.validate_take_item("marcus", "special_key");
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

TEST_F(ToolRegistryTest, TakeItemAccepted_NonKeyItem) {
    ToolRegistry reg(world);
    auto result = reg.validate_take_item("marcus", "crumpled_note");
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::TakeItemFromPlayer);
    EXPECT_EQ(req.actor_id, "marcus");
    EXPECT_EQ(req.params.at("item_id"), "crumpled_note");
}

TEST_F(ToolRegistryTest, TakeItemRejected_PlayerLacksItem) {
    ToolRegistry reg(world);
    auto result = reg.validate_take_item("marcus", "cargo_manifest");
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

// ---------------------------------------------------------------------------
// UpdateTrust tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, UpdateTrustClampsPositive) {
    ToolRegistry reg(world);
    auto result = reg.validate_update_trust("marcus", 150);
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::UpdateNpcTrust);
    EXPECT_EQ(req.params.at("delta"), "100");
}

TEST_F(ToolRegistryTest, UpdateTrustClampsNegative) {
    ToolRegistry reg(world);
    auto result = reg.validate_update_trust("marcus", -150);
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::UpdateNpcTrust);
    EXPECT_EQ(req.params.at("delta"), "-100");
}

TEST_F(ToolRegistryTest, UpdateTrustNormalDelta) {
    ToolRegistry reg(world);
    auto result = reg.validate_update_trust("marcus", 10);
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.params.at("delta"), "10");
}

// ---------------------------------------------------------------------------
// UpdateMood tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, UpdateMoodRejected_Invalid) {
    ToolRegistry reg(world);
    auto result = reg.validate_update_mood("marcus", "happy");
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

TEST_F(ToolRegistryTest, UpdateMoodAccepted_Valid) {
    ToolRegistry reg(world);
    auto result = reg.validate_update_mood("marcus", "suspicious");
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::UpdateNpcMood);
    EXPECT_EQ(req.params.at("mood"), "suspicious");
}

// ---------------------------------------------------------------------------
// MoveNpc tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, MoveNpcAccepted) {
    ToolRegistry reg(world);
    auto result = reg.validate_move_npc("marcus", "market_square");
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::MoveNpc);
    EXPECT_EQ(req.params.at("location_id"), "market_square");
}

TEST_F(ToolRegistryTest, MoveNpcRejected_BadLocation) {
    ToolRegistry reg(world);
    auto result = reg.validate_move_npc("marcus", "nowhere");
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

// ---------------------------------------------------------------------------
// RevealKnowledge tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, RevealKnowledgeAccepted) {
    ToolRegistry reg(world);
    auto result = reg.validate_reveal_knowledge("marcus", "fact_stolen_cargo");
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::RevealKnowledge);
    EXPECT_EQ(req.params.at("fact_id"), "fact_stolen_cargo");
}

TEST_F(ToolRegistryTest, RevealKnowledgeRejected_UnknownFact) {
    ToolRegistry reg(world);
    auto result = reg.validate_reveal_knowledge("marcus", "fact_unknown");
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

// ---------------------------------------------------------------------------
// AddMemory tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, AddMemoryAccepted) {
    ToolRegistry reg(world);
    auto result = reg.validate_add_memory("marcus", "  Player asked about cargo  ", 7);
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::AddMemory);
    EXPECT_EQ(req.params.at("summary"), "Player asked about cargo");
    EXPECT_EQ(req.params.at("importance"), "7");
}

TEST_F(ToolRegistryTest, AddMemoryRejected_EmptySummary) {
    ToolRegistry reg(world);
    auto result = reg.validate_add_memory("marcus", "", 5);
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

TEST_F(ToolRegistryTest, AddMemoryRejected_WhitespaceSummary) {
    ToolRegistry reg(world);
    auto result = reg.validate_add_memory("marcus", "   \t", 5);
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

TEST_F(ToolRegistryTest, AddMemoryClamps_Importance) {
    ToolRegistry reg(world);
    auto result = reg.validate_add_memory("marcus", "test", 15);
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.params.at("importance"), "10");
}

TEST_F(ToolRegistryTest, AddMemoryClampsLowImportance) {
    ToolRegistry reg(world);
    auto result = reg.validate_add_memory("marcus", "test", -4);
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.params.at("importance"), "1");
}

// ---------------------------------------------------------------------------
// SetFlag tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, SetFlagAccepted_Known) {
    ToolRegistry reg(world);
    auto result = reg.validate_set_flag("intro_complete", false);
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.type, MutationRequest::Type::SetFlag);
    EXPECT_TRUE(req.actor_id.empty());
    EXPECT_EQ(req.params.at("flag_id"), "intro_complete");
    EXPECT_EQ(req.params.at("value"), "false");
}

TEST_F(ToolRegistryTest, SetFlagRejected_Unknown) {
    ToolRegistry reg(world);
    auto result = reg.validate_set_flag("nonexistent", true);
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

// ---------------------------------------------------------------------------
// Cross-cutting: nonexistent NPC
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, NonexistentNpcRejected) {
    ToolRegistry reg(world);
    auto result = reg.validate_give_item("nobody", "tavern_key");
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_FALSE(std::get<std::string>(result).empty());
}

// ---------------------------------------------------------------------------
// Cross-cutting: all error strings are non-empty
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, ErrorStringsNonEmpty) {
    ToolRegistry reg(world);

    // Collect all expected error cases
    std::vector<ToolRegistry::ValidationResult> errors = {
        reg.validate_give_item("marcus", "crumpled_note"),
        reg.validate_give_item("marcus", "nonexistent"),
        reg.validate_give_item("nobody", "tavern_key"),
        reg.validate_take_item("marcus", "cargo_manifest"),
        reg.validate_update_mood("marcus", "happy"),
        reg.validate_move_npc("marcus", "nowhere"),
        reg.validate_reveal_knowledge("marcus", "fact_unknown"),
        reg.validate_add_memory("marcus", "", 5),
        reg.validate_set_flag("nonexistent", true),
    };

    for (std::size_t i = 0; i < errors.size(); ++i) {
        SCOPED_TRACE("Error case index " + std::to_string(i));
        ASSERT_TRUE(std::holds_alternative<std::string>(errors[i]));
        EXPECT_FALSE(std::get<std::string>(errors[i]).empty());
    }
}

// ---------------------------------------------------------------------------
// Pending mutation queue tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, RegisterPushesToPendingQueue) {
    ToolRegistry reg(world);
    EXPECT_TRUE(reg.pending_mutations().empty());

    auto err1 = reg.register_give_item("marcus", "cargo_manifest");
    EXPECT_FALSE(err1.has_value());
    EXPECT_EQ(reg.pending_mutations().size(), 1u);

    auto err2 = reg.register_update_mood("marcus", "suspicious");
    EXPECT_FALSE(err2.has_value());
    EXPECT_EQ(reg.pending_mutations().size(), 2u);
}

TEST_F(ToolRegistryTest, RegisterFailureDoesNotEnqueue) {
    ToolRegistry reg(world);
    auto err = reg.register_give_item("marcus", "crumpled_note");
    EXPECT_TRUE(err.has_value());
    EXPECT_TRUE(reg.pending_mutations().empty());
}

TEST_F(ToolRegistryTest, ClearPendingEmptiesQueue) {
    ToolRegistry reg(world);
    reg.register_give_item("marcus", "cargo_manifest");
    EXPECT_EQ(reg.pending_mutations().size(), 1u);
    reg.clear_pending();
    EXPECT_TRUE(reg.pending_mutations().empty());
}

TEST_F(ToolRegistryTest, SayDoesNotEnqueue) {
    ToolRegistry reg(world);
    EXPECT_FALSE(reg.register_say("marcus", "Hello, traveler."));
    EXPECT_TRUE(reg.pending_mutations().empty());
}

TEST_F(ToolRegistryTest, DrainDialogueReturnsCapturedSayTextAndClearsLog) {
    ToolRegistry reg(world);
    EXPECT_FALSE(reg.register_say("marcus", "Hello, traveler."));
    EXPECT_FALSE(reg.register_say("marcus", "Stay a while."));

    auto dialogue = reg.drain_dialogue_log();

    ASSERT_EQ(dialogue.size(), 2u);
    EXPECT_EQ(dialogue[0].first, "marcus");
    EXPECT_EQ(dialogue[0].second, "Hello, traveler.");
    EXPECT_TRUE(reg.drain_dialogue_log().empty());
}

TEST_F(ToolRegistryTest, RegisterGiveItemFailsWhenApplyPreconditionsNoLongerHold) {
    ToolRegistry reg(world);
    ASSERT_FALSE(reg.register_give_item("marcus", "cargo_manifest"));
    ASSERT_EQ(reg.pending_mutations().size(), 1u);

    ASSERT_TRUE(apply_mutation(world, reg.pending_mutations().front()));
    reg.clear_pending();

    ASSERT_TRUE(reg.register_give_item("marcus", "cargo_manifest"));
    EXPECT_TRUE(reg.pending_mutations().empty());
}

TEST_F(ToolRegistryTest, RegisterMultipleToolTypes) {
    ToolRegistry reg(world);
    reg.register_give_item("marcus", "cargo_manifest");
    reg.register_update_trust("marcus", 10);
    reg.register_move_npc("marcus", "market_square");
    reg.register_set_flag("intro_complete", false);
    EXPECT_EQ(reg.pending_mutations().size(), 4u);

    EXPECT_EQ(reg.pending_mutations()[0].type, MutationRequest::Type::GiveItemToPlayer);
    EXPECT_EQ(reg.pending_mutations()[1].type, MutationRequest::Type::UpdateNpcTrust);
    EXPECT_EQ(reg.pending_mutations()[2].type, MutationRequest::Type::MoveNpc);
    EXPECT_EQ(reg.pending_mutations()[3].type, MutationRequest::Type::SetFlag);
}

// ---------------------------------------------------------------------------
// Strict bool parsing (set_flag tool lambda)
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, SetFlagStrictBoolRejectsGarbage) {
    ToolRegistry reg(world);

    auto result = reg.handle_set_flag_tool("intro_complete", "maybe");

    EXPECT_NE(result.find("Error:"), std::string::npos);
    EXPECT_TRUE(reg.pending_mutations().empty());
}

// ---------------------------------------------------------------------------
// TakeItem readable feedback tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, TakeItemReturnsReadableText) {
    ToolRegistry reg(world);
    auto result = reg.handle_take_item_tool("marcus", "crumpled_note");
    EXPECT_NE(result.find("OK."), std::string::npos);
    EXPECT_NE(result.find("crumpled note"), std::string::npos);
    EXPECT_NE(result.find("Don't trust the innkeeper."), std::string::npos);
}

TEST_F(ToolRegistryTest, TakeItemReturnsNonReadableDetails) {
    // Add a non-readable item to the player's inventory
    Item potion;
    potion.id = "health_potion";
    potion.name = "Health Potion";
    potion.description = "A bubbling red liquid.";
    world.items["health_potion"] = potion;
    world.player.inventory.push_back("health_potion");

    ToolRegistry reg(world);
    auto result = reg.handle_take_item_tool("marcus", "health_potion");
    EXPECT_NE(result.find("OK."), std::string::npos);
    EXPECT_NE(result.find("Health Potion"), std::string::npos);
    EXPECT_NE(result.find("A bubbling red liquid."), std::string::npos);
    EXPECT_EQ(result.find("It reads:"), std::string::npos);
}

TEST_F(ToolRegistryTest, TakeItemToolReturnsErrorOnFailure) {
    ToolRegistry reg(world);
    auto result = reg.handle_take_item_tool("marcus", "cargo_manifest");
    EXPECT_NE(result.find("Error:"), std::string::npos);
}

// ---------------------------------------------------------------------------
// InspectItem tests (read-only tool, no mutations)
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, InspectItemReturnsReadableText) {
    ToolRegistry reg(world);
    auto result = reg.handle_inspect_item_tool("marcus", "cargo_manifest");
    EXPECT_NE(result.find("Cargo Manifest"), std::string::npos);
}

TEST_F(ToolRegistryTest, InspectItemReadableIncludesText) {
    // Give marcus the crumpled_note so he can inspect it
    world.npcs["marcus"].state.inventory.push_back("crumpled_note");

    ToolRegistry reg(world);
    auto result = reg.handle_inspect_item_tool("marcus", "crumpled_note");
    EXPECT_NE(result.find("crumpled note"), std::string::npos);
    EXPECT_NE(result.find("Don't trust the innkeeper."), std::string::npos);
    EXPECT_NE(result.find("It reads:"), std::string::npos);
}

TEST_F(ToolRegistryTest, InspectItemNonReadableOmitsText) {
    ToolRegistry reg(world);
    auto result = reg.handle_inspect_item_tool("marcus", "tavern_key");
    EXPECT_NE(result.find("tavern key"), std::string::npos);
    EXPECT_EQ(result.find("It reads:"), std::string::npos);
}

TEST_F(ToolRegistryTest, InspectItemRejected_NpcLacksItem) {
    ToolRegistry reg(world);
    auto result = reg.handle_inspect_item_tool("marcus", "crumpled_note");
    EXPECT_NE(result.find("Error:"), std::string::npos);
    EXPECT_NE(result.find("do not have"), std::string::npos);
}

TEST_F(ToolRegistryTest, InspectItemRejected_ItemNotExist) {
    ToolRegistry reg(world);
    auto result = reg.handle_inspect_item_tool("marcus", "nonexistent");
    EXPECT_NE(result.find("Error:"), std::string::npos);
}

TEST_F(ToolRegistryTest, InspectItemDoesNotEnqueue) {
    ToolRegistry reg(world);
    reg.handle_inspect_item_tool("marcus", "cargo_manifest");
    EXPECT_TRUE(reg.pending_mutations().empty());
}

// ---------------------------------------------------------------------------
// Per-NPC tool policy tests
// ---------------------------------------------------------------------------

TEST_F(ToolRegistryTest, ToolPolicyRejectsDisallowedToolBeforeWorldValidation) {
    world.npcs.at("marcus").identity.tool_policy.allowed_tools = {"take_item"};

    ToolRegistry reg(world);
    auto result = reg.validate_give_item("marcus", "cargo_manifest");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_NE(std::get<std::string>(result).find("not allowed"), std::string::npos);
}

TEST_F(ToolRegistryTest, ToolPolicyAllowsListedTool) {
    world.npcs.at("marcus").identity.tool_policy.allowed_tools = {"give_item"};

    ToolRegistry reg(world);
    auto result = reg.validate_give_item("marcus", "cargo_manifest");

    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
}

TEST_F(ToolRegistryTest, ToolPolicyRejectsItemOutsideScope) {
    world.npcs.at("marcus").identity.tool_policy.allowed_tools = {"give_item"};
    world.npcs.at("marcus").identity.tool_policy.allowed_items = {"cargo_manifest"};

    ToolRegistry reg(world);
    auto result = reg.validate_give_item("marcus", "tavern_key");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_NE(std::get<std::string>(result).find("not allowed to use item"), std::string::npos);
}

TEST_F(ToolRegistryTest, ToolPolicyRejectsFactOutsideScope) {
    world.npcs.at("marcus").identity.tool_policy.allowed_tools = {"reveal_knowledge"};
    world.npcs.at("marcus").identity.tool_policy.allowed_facts = {"fact_stolen_cargo"};

    ToolRegistry reg(world);
    auto result = reg.validate_reveal_knowledge("marcus", "fact_thief_identity");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_NE(std::get<std::string>(result).find("not allowed to use fact"), std::string::npos);
}

TEST_F(ToolRegistryTest, ToolPolicyRejectsLocationOutsideScope) {
    world.npcs.at("marcus").identity.tool_policy.allowed_tools = {"move_self"};
    world.npcs.at("marcus").identity.tool_policy.allowed_locations = {"tavern"};

    ToolRegistry reg(world);
    auto result = reg.validate_move_npc("marcus", "market_square");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_NE(std::get<std::string>(result).find("not allowed to use location"), std::string::npos);
}

TEST_F(ToolRegistryTest, ToolPolicyRejectsFlagOutsideScopeOnToolPath) {
    world.flags["secret_opened"] = false;
    world.npcs.at("marcus").identity.tool_policy.allowed_tools = {"set_flag"};
    world.npcs.at("marcus").identity.tool_policy.allowed_flags = {"secret_opened"};

    ToolRegistry reg(world);
    reg.set_active_npc_id("marcus");
    auto result = reg.handle_set_flag_tool("intro_complete", "false");

    EXPECT_NE(result.find("not allowed to use flag"), std::string::npos);
    EXPECT_TRUE(reg.pending_mutations().empty());
}

} // namespace chronicle
