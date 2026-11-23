// Scripted event conditions and actions.
#include <functional>
#include <gtest/gtest.h>

#include "chronicle/game/cartridge_game.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;

class EventsTest : public ::testing::Test {
  protected:
    EventsTest() : saves_("eventsaves") {}

    void install_event(const std::string &id, EventTriggerData event,
                       std::function<void(WorldState &)> configure = {}) {
        WorldState world = ct::make_test_world();
        world.events.clear();
        world.events[id] = std::move(event);
        if (configure) {
            configure(world);
        }
        game_ = std::make_unique<CartridgeGame>(std::move(world), saves_.path());
    }

    [[nodiscard]] GameEvents fire() { return game_->after_turn(); }

    ct::TempDir saves_;
    std::unique_ptr<CartridgeGame> game_;
};

TEST_F(EventsTest, PlayerAtConditionAndNarrateAction) {
    install_event("welcome",
                  {.conditions = {{.type = "player_at", .args = {"hall"}}},
                   .actions = {{.type = "narrate", .params = {{"text", "Dust swirls."}}}},
                   .once = true,
                   .fired = false});
    const auto events = fire();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().text, "Dust swirls.");
    // once=true means it never fires again.
    EXPECT_TRUE(fire().empty());
}

TEST_F(EventsTest, RepeatingEventFiresOncePerTurnOfTheClock) {
    install_event("hum", {.conditions = {{.type = "player_at", .args = {"hall"}}},
                          .actions = {{.type = "narrate", .params = {{"text", "A low hum."}}}},
                          .once = false,
                          .fired = false});
    for (const std::string line : {"look", "help", "inventory", "save 1", "load 1", "dance"}) {
        (void)game_->dispatch_player(line);
        EXPECT_TRUE(fire().empty()) << line;
    }
    for (const std::string line : {"take old coin", "drop old coin", "take old coin"}) {
        (void)game_->dispatch_player(line);
        EXPECT_EQ(fire().size(), 1u) << line;
    }
}

TEST_F(EventsTest, UnmetConditionDoesNotFire) {
    install_event("garden_only",
                  {.conditions = {{.type = "player_at", .args = {"garden"}}},
                   .actions = {{.type = "narrate", .params = {{"text", "Leaves rustle."}}}},
                   .once = true,
                   .fired = false});
    EXPECT_TRUE(fire().empty());
}

TEST_F(EventsTest, ConditionsAreAnded) {
    install_event("both", {.conditions = {{.type = "player_at", .args = {"hall"}},
                                          {.type = "flag_set", .args = {"gate_seen", "true"}}},
                           .actions = {{.type = "narrate", .params = {{"text", "Now."}}}},
                           .once = true,
                           .fired = false});
    EXPECT_TRUE(fire().empty());
    ASSERT_TRUE(game_->submit_world_action(
        actions::SetFlag{.flag_id = "gate_seen", .value = true, .significant = false}));
    EXPECT_EQ(fire().size(), 1u);
}

TEST_F(EventsTest, ClockIsCondition) {
    install_event("afternoon",
                  {.conditions = {{.type = "clock_is", .args = {"afternoon"}}},
                   .actions = {{.type = "narrate", .params = {{"text", "Shadows lengthen."}}}},
                   .once = true,
                   .fired = false});
    EXPECT_TRUE(fire().empty());
    ASSERT_TRUE(game_->submit_world_action(actions::AdvanceClock{.turns = 2}));
    EXPECT_EQ(fire().size(), 1u);
}

TEST_F(EventsTest, NpcTrustGeCondition) {
    install_event("trusted", {.conditions = {{.type = "npc_trust_ge", .args = {"keeper", "50"}}},
                              .actions = {{.type = "narrate", .params = {{"text", "A nod."}}}},
                              .once = true,
                              .fired = false});
    EXPECT_TRUE(fire().empty());
    ASSERT_TRUE(
        game_->submit_world_action(actions::AdjustNpcTrust{.npc_id = "keeper", .delta = 50}));
    EXPECT_EQ(fire().size(), 1u);
}

TEST_F(EventsTest, NpcAtAndItemInInventoryConditions) {
    install_event("ready", {.conditions = {{.type = "npc_at", .args = {"keeper", "hall"}},
                                           {.type = "item_in_player_inv", .args = {"old_coin"}}},
                            .actions = {{.type = "narrate", .params = {{"text", "Ready."}}}},
                            .once = true,
                            .fired = false});
    EXPECT_TRUE(fire().empty());
    (void)game_->dispatch_player("take old coin");
    EXPECT_FALSE(fire().empty());
}

TEST_F(EventsTest, TurnGeConditionViaSignificantTurns) {
    install_event("later", {.conditions = {{.type = "turn_ge", .args = {"1"}}},
                            .actions = {{.type = "narrate", .params = {{"text", "Time passes."}}}},
                            .once = true,
                            .fired = false});
    EXPECT_TRUE(fire().empty()); // turn 0
    (void)game_->dispatch_player("take old coin");
    const auto events = fire(); // clock advances to 1, then events run
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().text, "Time passes.");
}

TEST_F(EventsTest, MoveNpcAction) {
    install_event("summon",
                  {.conditions = {},
                   .actions = {{.type = "move_npc",
                                .params = {{"npc_id", "keeper"}, {"location_id", "garden"}}}},
                   .once = true,
                   .fired = false});
    const auto events = fire();
    EXPECT_EQ(game_->world().npcs.at("keeper").state.current_location, "garden");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().text, "Keeper moves to Garden.");
    EXPECT_TRUE(game_->world().item_positions.at("keepsake").is_npc("keeper"));
}

TEST_F(EventsTest, MovingActiveNpcEndsConversation) {
    install_event("departure",
                  {.conditions = {},
                   .actions = {{.type = "move_npc",
                                .params = {{"npc_id", "keeper"}, {"location_id", "garden"}}}},
                   .once = true,
                   .fired = false});
    (void)game_->dispatch_player("talk keeper");
    ASSERT_EQ(game_->phase(), GamePhase::in_conversation);
    (void)fire();
    EXPECT_EQ(game_->phase(), GamePhase::playing);
    EXPECT_FALSE(game_->active_npc_id().has_value());
}

TEST_F(EventsTest, MovingTheActiveNpcToTheSameRoomKeepsTheConversation) {
    install_event("stay", {.conditions = {},
                           .actions = {{.type = "move_npc",
                                        .params = {{"npc_id", "keeper"}, {"location_id", "hall"}}}},
                           .once = true,
                           .fired = false});
    (void)game_->dispatch_player("talk keeper");
    (void)fire();
    EXPECT_EQ(game_->phase(), GamePhase::in_conversation);
    EXPECT_EQ(game_->active_npc_id(), "keeper");
}

TEST_F(EventsTest, SetFlagActionRequiresBoolean) {
    install_event("mark", {.conditions = {},
                           .actions = {{.type = "set_flag",
                                        .params = {{"flag_id", "gate_seen"}, {"value", true}}}},
                           .once = true,
                           .fired = false});
    (void)fire();
    EXPECT_TRUE(game_->world().flags.at("gate_seen"));
}

TEST_F(EventsTest, SpawnItemAction) {
    // The keepsake starts in the keeper's inventory; spawning relocates it.
    install_event("plant",
                  {.conditions = {},
                   .actions = {{.type = "spawn_item",
                                .params = {{"item_id", "keepsake"}, {"location_id", "garden"}}}},
                   .once = true,
                   .fired = false});
    const auto events = fire();
    EXPECT_TRUE(game_->world().item_positions.at("keepsake").is_location("garden"));
    EXPECT_TRUE(items_at(game_->world(), ItemHolder::npc, "keeper").empty());
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().text, "Something new appears in Garden.");
}

TEST_F(EventsTest, SpawningLeavesWhatThePlayerCarries) {
    install_event("drip",
                  {.conditions = {},
                   .actions = {{.type = "spawn_item",
                                .params = {{"item_id", "old_coin"}, {"location_id", "garden"}}}},
                   .once = false,
                   .fired = false});
    (void)game_->dispatch_player("take old coin");
    EXPECT_TRUE(fire().empty());
    EXPECT_TRUE(game_->world().item_positions.at("old_coin").is_player());

    (void)game_->dispatch_player("drop old coin");
    EXPECT_EQ(fire().size(), 1u);
    EXPECT_TRUE(game_->world().item_positions.at("old_coin").is_location("garden"));
    // It is there already: nothing new appears.
    (void)game_->dispatch_player("take gate key");
    EXPECT_TRUE(fire().empty());
}

TEST_F(EventsTest, AnEventAnEarlierOneUndidDoesNotFire) {
    install_event(
        "a_first",
        {.conditions = {},
         .actions = {{.type = "set_flag", .params = {{"flag_id", "gate_seen"}, {"value", true}}}},
         .once = true,
         .fired = false},
        [](WorldState &world) {
            world.events["b_second"] = EventTriggerData{
                .conditions = {{.type = "flag_set", .args = {"gate_seen", "false"}}},
                .actions = {{.type = "narrate", .params = {{"text", "Unseen."}}}},
            };
        });
    EXPECT_TRUE(fire().empty());
    EXPECT_FALSE(game_->world().events.at("b_second").fired);
}

TEST_F(EventsTest, AnEventSetUpByAnotherWaitsForTheNextPass) {
    // In either id order.
    for (const std::string second : {"a_second", "z_second"}) {
        install_event("m_first",
                      {.conditions = {},
                       .actions = {{.type = "set_flag",
                                    .params = {{"flag_id", "gate_seen"}, {"value", true}}}},
                       .once = true,
                       .fired = false},
                      [&](WorldState &world) {
                          world.events[second] = EventTriggerData{
                              .conditions = {{.type = "flag_set", .args = {"gate_seen", "true"}}},
                              .actions = {{.type = "narrate", .params = {{"text", "Seen."}}}},
                          };
                      });
        EXPECT_TRUE(fire().empty()) << second;
        EXPECT_EQ(fire().size(), 1u) << second;
    }
}

TEST_F(EventsTest, OnlyOneEndingIsShownAndNothingFollowsIt) {
    install_event(
        "a_win",
        {.conditions = {},
         .actions = {{.type = "end_game", .params = {{"text", "You win."}}},
                     {.type = "narrate", .params = {{"text", "Afterwards."}}},
                     {.type = "set_flag", .params = {{"flag_id", "gate_seen"}, {"value", true}}}},
         .once = true,
         .fired = false},
        [](WorldState &world) {
            world.clock.turns_elapsed = 5;
            world.events["b_lose"] = EventTriggerData{
                .conditions = {},
                .actions = {{.type = "end_game", .params = {{"text", "You lose."}}}},
            };
        });
    (void)game_->dispatch_player("take old coin"); // The last turn of the clock.
    const auto events = fire();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().text, "You win.");
    EXPECT_FALSE(game_->world().flags.at("gate_seen"));
}

TEST_F(EventsTest, EndGameAction) {
    install_event("finale", {.conditions = {},
                             .actions = {{.type = "end_game", .params = {{"text", "It is done."}}}},
                             .once = true,
                             .fired = false});
    const auto events = fire();
    EXPECT_EQ(game_->phase(), GamePhase::game_over);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().kind, EventKind::ending);
    EXPECT_EQ(events.front().text, "It is done.");
}

TEST_F(EventsTest, EndGameDefaultText) {
    install_event("finale", {.conditions = {},
                             .actions = {{.type = "end_game", .params = nlohmann::json::object()}},
                             .once = true,
                             .fired = false});
    const auto events = fire();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().text, "The scenario ends.");
}

TEST_F(EventsTest, AuthoredEndingAtDeadlineWinsOverFallbackEnding) {
    install_event("deadline",
                  {.conditions = {{.type = "turn_ge", .args = {"6"}}},
                   .actions = {{.type = "end_game", .params = {{"text", "The authored ending."}}}},
                   .once = true,
                   .fired = false},
                  [](WorldState &world) { world.clock.turns_elapsed = 5; });
    (void)game_->dispatch_player("take old coin");
    const auto events = fire();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().text, "The authored ending.");
}

} // namespace
} // namespace chronicle
