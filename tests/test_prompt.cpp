#include <gtest/gtest.h>

#include "chronicle/prompt.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;

TEST(Prompt, SystemPromptCarriesIdentityKnowledgeAndRules) {
    const WorldState world = ct::make_test_world();
    const std::string prompt = build_npc_system_prompt(world, "keeper");
    EXPECT_NE(prompt.find("You are Keeper, Groundskeeper."), std::string::npos);
    EXPECT_NE(prompt.find("Personality: Wary but fair."), std::string::npos);
    EXPECT_NE(prompt.find("- Protect the grounds"), std::string::npos);
    EXPECT_NE(prompt.find("[fact_gate] The gate was locked after the accident."),
              std::string::npos);
    EXPECT_NE(prompt.find("Stay in character."), std::string::npos);
    // The secret itself never appears in the static prompt.
    EXPECT_EQ(prompt.find("grave"), std::string::npos);
}

TEST(Prompt, EmptyKnowledgeShowsNone) {
    WorldState world = ct::make_test_world();
    world.npcs.at("keeper").identity.knowledge.clear();
    const std::string prompt = build_npc_system_prompt(world, "keeper");
    EXPECT_NE(prompt.find("Knowledge:\n- (none)"), std::string::npos);
}

TEST(Prompt, TurnMessageCarriesWorldStateAndPlayerPayload) {
    WorldState world = ct::make_test_world();
    world.player.inventory.push_back("old_coin");
    const std::string message = build_npc_turn_message(world, "keeper", "who are you?");
    EXPECT_NE(message.find("Time: morning (turn 0)"), std::string::npos);
    EXPECT_NE(message.find("Your location: Hall"), std::string::npos);
    EXPECT_NE(message.find("Mood: neutral"), std::string::npos);
    EXPECT_NE(message.find("Trust toward player: 0"), std::string::npos);
    EXPECT_NE(message.find("Also here: no one"), std::string::npos);
    EXPECT_NE(message.find("Gate Key"), std::string::npos);
    // Hidden items are not shown to the NPC either.
    EXPECT_EQ(message.find("Hidden Note"), std::string::npos);
    EXPECT_NE(message.find("\"player_said\":\"who are you?\""), std::string::npos);
    EXPECT_NE(message.find("Old Coin"), std::string::npos);
}

TEST(Prompt, SecretAppearsOnlyAboveTrustThreshold) {
    WorldState world = ct::make_test_world();
    EXPECT_EQ(build_npc_turn_message(world, "keeper", "hi").find("grave"), std::string::npos);
    world.npcs.at("keeper").state.trust_toward_player = 60;
    EXPECT_NE(build_npc_turn_message(world, "keeper", "hi")
                  .find("Secret you may reveal carefully: The garden hides a grave."),
              std::string::npos);
}

TEST(Prompt, MemoriesSortedByImportanceAndBudgeted) {
    WorldState world = ct::make_test_world();
    auto &memories = world.npcs.at("keeper").state.memories;
    memories.push_back({.timestamp = "",
                        .type = "observation",
                        .summary = "minor detail",
                        .importance = 1,
                        .related_npc = "",
                        .related_item = ""});
    memories.push_back({.timestamp = "",
                        .type = "observation",
                        .summary = "crucial secret meeting",
                        .importance = 9,
                        .related_npc = "",
                        .related_item = ""});
    const std::string message = build_npc_turn_message(world, "keeper", "hi");
    const auto crucial = message.find("(9) crucial secret meeting");
    const auto minor = message.find("(1) minor detail");
    ASSERT_NE(crucial, std::string::npos);
    ASSERT_NE(minor, std::string::npos);
    EXPECT_LT(crucial, minor);

    // A tiny budget keeps the important memory and drops the minor one.
    world.config.max_memory_tokens = 20; // word budget of 5 fits only the first line
    const std::string tight = build_npc_turn_message(world, "keeper", "hi");
    EXPECT_NE(tight.find("crucial secret meeting"), std::string::npos);
    EXPECT_EQ(tight.find("minor detail"), std::string::npos);
}

} // namespace
} // namespace chronicle
