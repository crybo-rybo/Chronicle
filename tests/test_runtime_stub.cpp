// The console runtime in stub mode: no endpoint, no network, still playable.
#include <gtest/gtest.h>

#include "chronicle/cli.hpp"
#include "chronicle/runtime.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;

std::string joined(const GameEvents &events) {
    std::string all;
    for (const auto &event : events) {
        all += event.text + "\n";
    }
    return all;
}

class StubRuntimeTest : public ::testing::Test {
  protected:
    StubRuntimeTest()
        : saves_("stubsaves"), game_(ct::make_test_world(), saves_.path()),
          runtime_(game_, std::nullopt) {}

    ct::TempDir saves_;
    CartridgeGame game_;
    ConsoleRuntime runtime_;
};

TEST_F(StubRuntimeTest, UsesStubWithoutEndpoint) {
    EXPECT_TRUE(runtime_.using_stub());
}

TEST_F(StubRuntimeTest, ConversationTurnGoesThroughGate) {
    (void)runtime_.handle_line("talk keeper");
    const auto events = runtime_.handle_line("hello?");
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.front().kind, EventKind::dialogue);
    EXPECT_EQ(events.front().text, std::string("Keeper: \"") + STUB_REPLY + "\"");
}

TEST_F(StubRuntimeTest, SaylessNpcFallsBackToPlainDialogue) {
    WorldState world = ct::make_test_world();
    world.npcs.at("keeper").identity.tool_policy.allowed_tools = {"remember"};
    CartridgeGame game(std::move(world), saves_.path());
    ConsoleRuntime runtime(game, std::nullopt);
    (void)runtime.handle_line("talk keeper");
    const auto events = runtime.handle_line("hello?");
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.front().kind, EventKind::dialogue);
    EXPECT_NE(events.front().text.find(STUB_REPLY), std::string::npos);
}

TEST_F(StubRuntimeTest, EmptyLineProducesNothing) {
    EXPECT_TRUE(runtime_.handle_line("   ").empty());
}

TEST_F(StubRuntimeTest, AfterTurnEventsAppendedToCommands) {
    WorldState world = ct::make_test_world();
    world.events["chime"] = EventTriggerData{
        .conditions = {{.type = "turn_ge", .args = {"1"}}},
        .actions = {{.type = "narrate", .params = {{"text", "A clock chimes."}}}},
        .once = true,
        .fired = false,
    };
    CartridgeGame game(std::move(world), saves_.path());
    ConsoleRuntime runtime(game, std::nullopt);
    const auto events = runtime.handle_line("take old coin");
    const auto text = joined(events);
    EXPECT_NE(text.find("You take the Old Coin."), std::string::npos);
    EXPECT_NE(text.find("A clock chimes."), std::string::npos);
}

TEST_F(StubRuntimeTest, StartingConversationDoesNotAlsoRunNpcTurn) {
    const auto events = runtime_.handle_line("talk keeper");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().kind, EventKind::narration);
}

TEST_F(StubRuntimeTest, BlockedConversationCommandDoesNotReachNpc) {
    (void)runtime_.handle_line("talk keeper");
    const auto events = runtime_.handle_line("go north");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().kind, EventKind::narration);
    EXPECT_NE(events.front().text.find("Finish the conversation"), std::string::npos);
}

TEST_F(StubRuntimeTest, AliasedHardCommandIsRoutedExactlyOnce) {
    WorldState world = ct::make_test_world();
    world.config.verb_aliases["bag"] = "inventory";
    CartridgeGame game(std::move(world), saves_.path());
    ConsoleRuntime runtime(game, std::nullopt);
    (void)runtime.handle_line("talk keeper");
    const auto events = runtime.handle_line("bag");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().kind, EventKind::narration);
    EXPECT_NE(events.front().text.find("carrying nothing"), std::string::npos);
}

TEST_F(StubRuntimeTest, GameOverStopsFurtherProcessing) {
    const auto events = runtime_.handle_line("quit");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().kind, EventKind::system);
    EXPECT_EQ(game_.phase(), GamePhase::game_over);
}

TEST_F(StubRuntimeTest, FullStubPlaythroughOnTinyWorld) {
    ct::TempDir saves("tinysaves");
    CartridgeGame tiny(build_tiny_world(), saves.path());
    ConsoleRuntime runtime(tiny, std::nullopt);

    EXPECT_NE(joined(tiny.bootstrap()).find("Bare Room"), std::string::npos);
    EXPECT_NE(joined(runtime.handle_line("talk stranger")).find("Stranger"), std::string::npos);
    const auto reply = runtime.handle_line("who are you?");
    ASSERT_FALSE(reply.empty());
    EXPECT_EQ(reply.front().kind, EventKind::dialogue);
    (void)runtime.handle_line("bye");
    (void)runtime.handle_line("quit");
    EXPECT_EQ(tiny.phase(), GamePhase::game_over);
}

} // namespace
} // namespace chronicle
