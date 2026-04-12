#include "ai/tool_registry.hpp"
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
        manifest.name = "cargo manifest";
        manifest.key_item = false;
        world.items["cargo_manifest"] = manifest;

        Item note;
        note.id = "crumpled_note";
        note.name = "crumpled note";
        note.key_item = false;
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
    EXPECT_EQ(req.npc_id, "marcus");
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
    EXPECT_EQ(req.npc_id, "marcus");
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
    auto result = reg.validate_add_memory("marcus", "Player asked about cargo", 7);
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

TEST_F(ToolRegistryTest, AddMemoryClamps_Importance) {
    ToolRegistry reg(world);
    auto result = reg.validate_add_memory("marcus", "test", 15);
    ASSERT_TRUE(std::holds_alternative<MutationRequest>(result));
    auto &req = std::get<MutationRequest>(result);
    EXPECT_EQ(req.params.at("importance"), "10");
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
    EXPECT_TRUE(req.npc_id.empty());
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

} // namespace chronicle
