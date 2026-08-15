#include <gtest/gtest.h>

#include "chronicle/game/cartridge_game.hpp"
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

class GameTest : public ::testing::Test {
  protected:
    GameTest() : saves_("gamesaves"), game_(ct::make_test_world(), saves_.path()) {}

    ct::TempDir saves_;
    CartridgeGame game_;
};

TEST_F(GameTest, BootstrapShowsTitleLookAndHint) {
    const auto events = game_.bootstrap();
    ASSERT_GE(events.size(), 3u);
    EXPECT_EQ(events.front().kind, EventKind::title);
    EXPECT_NE(events.front().text.find("Test World"), std::string::npos);
    EXPECT_EQ(events[1].kind, EventKind::look);
    EXPECT_EQ(events.back().kind, EventKind::hint);
}

TEST_F(GameTest, LookShowsItemsNpcsExitsAndPeriod) {
    const auto text = joined(game_.handle_player("look"));
    EXPECT_NE(text.find("Hall"), std::string::npos);
    EXPECT_NE(text.find("Gate Key"), std::string::npos);
    EXPECT_NE(text.find("Keeper"), std::string::npos);
    EXPECT_NE(text.find("north (locked)"), std::string::npos);
    EXPECT_NE(text.find("[morning]"), std::string::npos);
}

TEST_F(GameTest, HiddenItemsAreNotShown) {
    const auto text = joined(game_.handle_player("look"));
    EXPECT_EQ(text.find("Hidden Note"), std::string::npos);
}

TEST_F(GameTest, GoUnknownDirection) {
    EXPECT_NE(joined(game_.handle_player("go west")).find("can't go that way"), std::string::npos);
    EXPECT_NE(joined(game_.handle_player("go")).find("Go where?"), std::string::npos);
}

TEST_F(GameTest, GoLockedDirectionBlocked) {
    EXPECT_NE(joined(game_.handle_player("go north")).find("That way is locked"),
              std::string::npos);
    EXPECT_EQ(game_.world().player.current_location, "hall");
}

TEST_F(GameTest, UnlockWithKeyThenGo) {
    EXPECT_NE(joined(game_.handle_player("take gate key")).find("You take the Gate Key"),
              std::string::npos);
    const auto unlock = joined(game_.handle_player("use gate key on north"));
    EXPECT_NE(unlock.find("The way is unlocked"), std::string::npos);
    const auto go = joined(game_.handle_player("go north"));
    EXPECT_NE(go.find("Garden"), std::string::npos);
    EXPECT_EQ(game_.world().player.current_location, "garden");
}

TEST_F(GameTest, UseWithoutCarryingItem) {
    EXPECT_NE(joined(game_.handle_player("use gate key on north")).find("aren't carrying"),
              std::string::npos);
}

TEST_F(GameTest, UseWrongTarget) {
    (void)game_.handle_player("take old coin");
    EXPECT_NE(joined(game_.handle_player("use old coin on north")).find("doesn't work"),
              std::string::npos);
}

TEST_F(GameTest, TakeDropInventoryFlow) {
    EXPECT_NE(joined(game_.handle_player("inventory")).find("carrying nothing"), std::string::npos);
    (void)game_.handle_player("take old coin");
    EXPECT_NE(joined(game_.handle_player("inventory")).find("Old Coin"), std::string::npos);
    EXPECT_EQ(game_.world().item_owners.at("old_coin"), "player");
    (void)game_.handle_player("drop old coin");
    EXPECT_NE(joined(game_.handle_player("inventory")).find("carrying nothing"), std::string::npos);
    EXPECT_EQ(game_.world().item_owners.at("old_coin"), "location");
}

TEST_F(GameTest, TakeRejectsUntakeableAndAbsent) {
    EXPECT_NE(joined(game_.handle_player("take statue")).find("can't take that"),
              std::string::npos);
    EXPECT_NE(joined(game_.handle_player("take moonbeam")).find("Take what?"), std::string::npos);
}

TEST_F(GameTest, ExamineReadableItem) {
    const auto text = joined(game_.handle_player("examine old coin"));
    EXPECT_NE(text.find("A worn coin."), std::string::npos);
    EXPECT_NE(text.find("It reads: MDCCLX"), std::string::npos);
}

TEST_F(GameTest, ExamineAbsentItem) {
    EXPECT_NE(joined(game_.handle_player("examine moonbeam")).find("don't see that"),
              std::string::npos);
}

TEST_F(GameTest, TalkStartsConversationAndBellowsBlockedCommands) {
    const auto talk = joined(game_.handle_player("talk keeper"));
    EXPECT_NE(talk.find("You begin talking with Keeper"), std::string::npos);
    EXPECT_EQ(game_.phase(), GamePhase::in_conversation);
    EXPECT_EQ(game_.active_npc_id(), "keeper");
    EXPECT_TRUE(game_.world().npcs.at("keeper").state.has_met_player);

    // Movement commands are blocked mid-conversation; hard commands are not.
    EXPECT_NE(joined(game_.handle_player("go north")).find("Finish the conversation"),
              std::string::npos);
    EXPECT_NE(joined(game_.handle_player("inventory")).find("carrying nothing"), std::string::npos);

    // Free text produces no immediate events (the LLM path owns it).
    EXPECT_TRUE(game_.handle_player("hello there").empty());
    EXPECT_TRUE(game_.wants_llm_turn("hello there"));
    EXPECT_FALSE(game_.wants_llm_turn("inventory"));
    EXPECT_FALSE(game_.wants_llm_turn("bye"));

    const auto bye = joined(game_.handle_player("bye"));
    EXPECT_NE(bye.find("You end the conversation"), std::string::npos);
    EXPECT_EQ(game_.phase(), GamePhase::playing);
}

TEST_F(GameTest, TalkToAbsentNpc) {
    (void)game_.submit_npc_tool("keeper", tools::MoveSelf{.location_id = "garden"});
    EXPECT_NE(joined(game_.handle_player("talk keeper")).find("isn't here"), std::string::npos);
    EXPECT_NE(joined(game_.handle_player("talk nobody")).find("Talk to whom?"), std::string::npos);
}

TEST_F(GameTest, VerbAliasesExpand) {
    game_.world().config.verb_aliases["grab"] = "take";
    (void)game_.handle_player("grab old coin");
    EXPECT_EQ(game_.world().item_owners.at("old_coin"), "player");
}

TEST_F(GameTest, UnknownCommandEchoes) {
    EXPECT_NE(joined(game_.handle_player("dance")).find("Unknown command: dance"),
              std::string::npos);
}

TEST_F(GameTest, QuitEndsGame) {
    const auto events = game_.handle_player("quit");
    EXPECT_EQ(events.front().kind, EventKind::system);
    EXPECT_EQ(game_.phase(), GamePhase::game_over);
    EXPECT_NE(joined(game_.handle_player("look")).find("scenario has ended"), std::string::npos);
}

TEST_F(GameTest, SignificantTurnsAdvanceClock) {
    EXPECT_EQ(game_.world().clock.turns_elapsed, 0);
    (void)game_.handle_player("look"); // not significant
    (void)game_.after_turn();
    EXPECT_EQ(game_.world().clock.turns_elapsed, 0);
    (void)game_.handle_player("take old coin"); // significant
    (void)game_.after_turn();
    EXPECT_EQ(game_.world().clock.turns_elapsed, 1);
}

TEST_F(GameTest, TimeExpiryEndsGame) {
    // turns_per_period=2, total_periods=3 -> expires at 6 elapsed turns.
    game_.world().clock.turns_elapsed = 5;
    (void)game_.handle_player("take old coin");
    const auto events = game_.after_turn();
    EXPECT_EQ(game_.phase(), GamePhase::game_over);
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back().kind, EventKind::ending);
    EXPECT_NE(events.back().text.find("Time has run out"), std::string::npos);
}

TEST_F(GameTest, SaveAndLoadRoundTrip) {
    (void)game_.handle_player("take old coin");
    (void)game_.handle_player("talk keeper");
    EXPECT_NE(joined(game_.handle_player("save 3")).find("Saved to slot 3"), std::string::npos);

    // Mutate further, then load the snapshot back.
    (void)game_.handle_player("bye");
    (void)game_.handle_player("drop old coin");
    const auto loaded = joined(game_.handle_player("load 3"));
    EXPECT_NE(loaded.find("Loaded slot 3"), std::string::npos);
    EXPECT_EQ(game_.phase(), GamePhase::in_conversation);
    EXPECT_EQ(game_.active_npc_id(), "keeper");
    EXPECT_EQ(game_.world().item_owners.at("old_coin"), "player");
}

TEST_F(GameTest, LoadMissingSlot) {
    EXPECT_NE(joined(game_.handle_player("load 7")).find("No save in slot 7"), std::string::npos);
}

} // namespace
} // namespace chronicle
