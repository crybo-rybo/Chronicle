#include <gtest/gtest.h>
#include <memory>

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

    [[nodiscard]] GameEvents command(const std::string &text) {
        return game_.dispatch_player(text).events;
    }

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
    const auto text = joined(command("look"));
    EXPECT_NE(text.find("Hall"), std::string::npos);
    EXPECT_NE(text.find("Gate Key"), std::string::npos);
    EXPECT_NE(text.find("Keeper"), std::string::npos);
    EXPECT_NE(text.find("north (locked)"), std::string::npos);
    EXPECT_NE(text.find("[morning]"), std::string::npos);
}

TEST_F(GameTest, HiddenItemsAreNotShown) {
    const auto text = joined(command("look"));
    EXPECT_EQ(text.find("Hidden Note"), std::string::npos);
}

TEST_F(GameTest, HiddenItemsCannotBeTakenOrExaminedByGuessedId) {
    EXPECT_NE(joined(command("take hidden_note")).find("don't see that"), std::string::npos);
    EXPECT_NE(joined(command("examine hidden_note")).find("don't see that"), std::string::npos);
    EXPECT_TRUE(game_.world().item_positions.at("hidden_note").is_location("hall"));
}

TEST_F(GameTest, GoUnknownDirection) {
    EXPECT_NE(joined(command("go west")).find("can't go that way"), std::string::npos);
    EXPECT_NE(joined(command("go")).find("Go where?"), std::string::npos);
}

TEST_F(GameTest, GoLockedDirectionBlocked) {
    EXPECT_NE(joined(command("go north")).find("That way is locked"), std::string::npos);
    EXPECT_EQ(game_.world().player.current_location, "hall");
}

TEST_F(GameTest, UnlockWithKeyThenGo) {
    EXPECT_NE(joined(command("take gate key")).find("You take the Gate Key"), std::string::npos);
    const auto unlock = joined(command("use gate key on north"));
    EXPECT_NE(unlock.find("The way is unlocked"), std::string::npos);
    const auto go = joined(command("go north"));
    EXPECT_NE(go.find("Garden"), std::string::npos);
    EXPECT_EQ(game_.world().player.current_location, "garden");
}

TEST_F(GameTest, UseWithoutCarryingItem) {
    EXPECT_NE(joined(command("use gate key on north")).find("aren't carrying"), std::string::npos);
}

TEST_F(GameTest, UseWrongTarget) {
    (void)command("take old coin");
    EXPECT_NE(joined(command("use old coin on north")).find("doesn't work"), std::string::npos);
}

TEST_F(GameTest, TakeDropInventoryFlow) {
    EXPECT_NE(joined(command("inventory")).find("carrying nothing"), std::string::npos);
    (void)command("take old coin");
    EXPECT_NE(joined(command("inventory")).find("Old Coin"), std::string::npos);
    EXPECT_TRUE(game_.world().item_positions.at("old_coin").is_player());
    (void)command("drop old coin");
    EXPECT_NE(joined(command("inventory")).find("carrying nothing"), std::string::npos);
    EXPECT_TRUE(game_.world().item_positions.at("old_coin").is_location("hall"));
}

TEST_F(GameTest, TakeRejectsUntakeableAndAbsent) {
    EXPECT_NE(joined(command("take statue")).find("can't take that"), std::string::npos);
    EXPECT_NE(joined(command("take moonbeam")).find("don't see that"), std::string::npos);
    EXPECT_NE(joined(command("take")).find("Take what?"), std::string::npos);
}

TEST_F(GameTest, ExamineReadableItem) {
    const auto text = joined(command("examine old coin"));
    EXPECT_NE(text.find("A worn coin."), std::string::npos);
    EXPECT_NE(text.find("It reads: MDCCLX"), std::string::npos);
}

TEST_F(GameTest, OnlyAReadableItemIsRead) {
    for (const std::string readable : {"false", ""}) {
        WorldState world = ct::make_test_world();
        world.items.at("old_coin").properties = {{"readable", readable}, {"text", "MDCCLX"}};
        CartridgeGame game(std::move(world), saves_.path());
        EXPECT_EQ(joined(game.dispatch_player("examine old coin").events), "A worn coin.\n");
    }
}

TEST_F(GameTest, ExamineAbsentItem) {
    EXPECT_NE(joined(command("examine moonbeam")).find("don't see that"), std::string::npos);
}

TEST_F(GameTest, TalkStartsConversationAndBellowsBlockedCommands) {
    const auto talk = joined(command("talk keeper"));
    EXPECT_NE(talk.find("You begin talking with Keeper"), std::string::npos);
    EXPECT_EQ(game_.phase(), GamePhase::in_conversation);
    EXPECT_EQ(game_.active_npc_id(), "keeper");
    EXPECT_TRUE(game_.world().npcs.at("keeper").state.has_met_player);

    // Movement commands are blocked mid-conversation; hard commands are not.
    EXPECT_NE(joined(command("go north")).find("Finish the conversation"), std::string::npos);
    EXPECT_NE(joined(command("inventory")).find("carrying nothing"), std::string::npos);

    // Free text produces no immediate events (the LLM path owns it).
    const auto free_text = game_.dispatch_player("hello there");
    EXPECT_TRUE(free_text.events.empty());
    ASSERT_TRUE(free_text.npc_turn.has_value());
    EXPECT_EQ(free_text.npc_turn->npc_id, "keeper");
    EXPECT_EQ(free_text.npc_turn->player_text, "hello there");

    const auto bye = joined(command("bye"));
    EXPECT_NE(bye.find("You end the conversation"), std::string::npos);
    EXPECT_EQ(game_.phase(), GamePhase::playing);
}

// --- the action gate ----------------------------------------------------------------

TEST_F(GameTest, TheGateRefusesWhatTheWorldDoesNotHold) {
    using namespace actions;
    const auto at = [](const ItemHolder holder, const std::string &id) {
        return ItemPosition{.holder = holder, .id = id};
    };
    MemoryEntry blank;
    blank.summary = " ";
    MemoryEntry trivial;
    trivial.summary = "x";
    trivial.importance = 0;
    const std::vector<std::pair<WorldAction, std::string>> refused = {
        {MovePlayer{.direction = "west"}, "Unknown player exit"},
        {MovePlayer{.direction = "north"}, "Player exit is locked"},
        {RelocateItem{.item_id = "moon", .destination = at(ItemHolder::player, "")},
         "Unknown item"},
        {RelocateItem{.item_id = "old_coin", .destination = at(ItemHolder::location, "void")},
         "Unknown item destination"},
        {RelocateItem{.item_id = "old_coin", .destination = at(ItemHolder::npc, "ghost")},
         "Unknown item recipient"},
        {RelocateItem{.item_id = "old_coin", .destination = at(ItemHolder::player, "hall")},
         "Player item position must not carry an id"},
        {UnlockExit{.location_id = "void", .direction = "north"}, "Unknown exit"},
        {UnlockExit{.location_id = "hall", .direction = "west"}, "Unknown exit"},
        {UnlockExit{.location_id = "garden", .direction = "south"}, "Exit is not locked"},
        {BeginConversation{.npc_id = "ghost"}, "Unknown NPC"},
        {EndConversation{}, "No conversation is active"},
        {MarkEventFired{.event_id = "nope"}, "Unknown event"},
        {MoveNpc{.npc_id = "ghost", .destination = "hall"}, "Unknown NPC destination"},
        {MoveNpc{.npc_id = "keeper", .destination = "void"}, "Unknown NPC destination"},
        {UpdateNpcMood{.npc_id = "keeper", .mood = "angry"}, "Invalid NPC mood update"},
        {UpdateNpcMood{.npc_id = "ghost", .mood = "neutral"}, "Invalid NPC mood update"},
        {AdjustNpcTrust{.npc_id = "ghost", .delta = 1}, "Unknown NPC"},
        {RevealFact{.fact_id = "nope"}, "Unknown fact"},
        {AddMemory{.npc_id = "ghost", .memory = {}}, "Invalid NPC memory"},
        {AddMemory{.npc_id = "keeper", .memory = blank}, "Invalid NPC memory"},
        {AddMemory{.npc_id = "keeper", .memory = trivial}, "Invalid NPC memory"},
        {SetFlag{.flag_id = "nope", .value = true}, "Unknown flag"},
    };
    const nlohmann::json before = game_.world();
    for (const auto &[action, reason] : refused) {
        const auto outcome = game_.submit_world_action(action);
        ASSERT_FALSE(outcome.has_value()) << reason;
        EXPECT_EQ(outcome.error().reason, reason);
    }
    EXPECT_EQ(nlohmann::json(game_.world()), before);

    ASSERT_TRUE(game_.submit_world_action(MoveNpc{.npc_id = "keeper", .destination = "garden"}));
    EXPECT_FALSE(game_.submit_world_action(BeginConversation{.npc_id = "keeper"}));
}

TEST_F(GameTest, NothingChangesAfterTheGameHasEnded) {
    ASSERT_TRUE(game_.submit_world_action(actions::EndGame{}));
    const nlohmann::json before = game_.world();
    const auto outcome =
        game_.submit_world_action(actions::SetFlag{.flag_id = "gate_seen", .value = true});
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().reason, "The game has ended");
    EXPECT_EQ(nlohmann::json(game_.world()), before);
}

TEST_F(GameTest, AnInconsistentRestoreIsRefused) {
    const auto refused = [&](const std::string &reason, const auto &spoil) {
        auto checkpoint = game_.checkpoint_runtime();
        spoil(checkpoint);
        const auto outcome = game_.restore_runtime(std::move(checkpoint));
        ASSERT_FALSE(outcome.has_value()) << reason;
        EXPECT_EQ(outcome.error().reason, reason);
        EXPECT_EQ(game_.phase(), GamePhase::playing);
        EXPECT_TRUE(game_.world().npcs.contains("keeper"));
    };
    refused("Saved world failed validation",
            [](RuntimeCheckpoint &c) { c.world.player.current_location = "void"; });
    refused("Saved conversation state is inconsistent",
            [](RuntimeCheckpoint &c) { c.phase = GamePhase::in_conversation; });
    refused("Saved conversation state is inconsistent", [](RuntimeCheckpoint &c) {
        c.phase = GamePhase::in_conversation;
        c.active_npc = "ghost";
    });
    refused("Saved conversation state is inconsistent", [](RuntimeCheckpoint &c) {
        c.phase = GamePhase::in_conversation;
        c.active_npc = "keeper";
        c.world.npcs.at("keeper").state.current_location = "garden";
    });
    refused("Saved active NPC does not match the game phase",
            [](RuntimeCheckpoint &c) { c.active_npc = "keeper"; });
}

// --- what the player's words refer to ----------------------------------------------

class NounTest : public ::testing::Test {
  protected:
    NounTest() : saves_("nounsaves") {
        // A brass key out of reach, sorting before the gate key in the hall.
        add_item("brass_key", "Brass Key", "garden");
        add_item("monkey", "Monkey Puzzle", "hall");
    }

    void add_item(const std::string &item_id, const std::string &name,
                  const std::string &location_id) {
        ItemData item;
        item.name = name;
        item.description = "A " + name + ".";
        world_.items[item_id] = item;
        world_.item_positions[item_id] =
            ItemPosition{.holder = ItemHolder::location, .id = location_id};
    }

    void add_npc(const std::string &npc_id, const std::string &name,
                 const std::string &location_id) {
        NpcData npc;
        npc.identity.id = npc_id;
        npc.identity.name = name;
        npc.state.current_location = location_id;
        world_.npcs[npc_id] = npc;
    }

    [[nodiscard]] std::string command(const std::string &text) {
        if (!game_) {
            game_ = std::make_unique<CartridgeGame>(world_, saves_.path());
        }
        return joined(game_->dispatch_player(text).events);
    }

    ct::TempDir saves_;
    WorldState world_ = ct::make_test_world();
    std::unique_ptr<CartridgeGame> game_;
};

TEST_F(NounTest, WordsMeanWhatIsHere) {
    EXPECT_EQ(command("take key"), "You take the Gate Key.\n");
    EXPECT_EQ(command("examine key"), "A heavy iron key.\n");
    EXPECT_EQ(command("drop key"), "You drop the Gate Key.\n");
}

TEST_F(NounTest, ArticlesAndCaseDoNotMatter) {
    EXPECT_EQ(command("take the Gate KEY"), "You take the Gate Key.\n");
    EXPECT_EQ(command("drop a gate key"), "You drop the Gate Key.\n");
    EXPECT_EQ(command("take the"), "You don't see that here.\n");
}

TEST_F(NounTest, AnArticleInANameDoesNotMatterEither) {
    add_item("book", "The Ledger", "hall");
    add_item("ledger_stand", "Ledger Stand", "hall");
    EXPECT_EQ(command("take ledger"), "You take the The Ledger.\n");
    EXPECT_EQ(command("drop the ledger"), "You drop the The Ledger.\n");
}

TEST_F(NounTest, AmbiguousWordsAskWhichAndDoNothing) {
    add_item("silver_key", "Silver Key", "hall");
    add_item("iron_key", "Iron Key", "hall");
    EXPECT_EQ(command("take key"), "Which do you mean: Gate Key, Iron Key or Silver Key?\n");
    EXPECT_EQ(command("examine key"), "Which do you mean: Gate Key, Iron Key or Silver Key?\n");
    EXPECT_EQ(command("inventory"), "You are carrying nothing.\n");
    EXPECT_EQ(command("take silver"), "You take the Silver Key.\n");
    EXPECT_EQ(command("drop key"), "You drop the Silver Key.\n");
}

TEST_F(NounTest, AWholeNameBeatsAPartOfAnother) {
    add_item("key", "Key", "hall");
    EXPECT_EQ(command("take key"), "You take the Key.\n");
    EXPECT_EQ(command("take puzz"), "You take the Monkey Puzzle.\n");
}

TEST_F(NounTest, ThingsOfOneNameAreTakenOneByOne) {
    add_item("coin_two", "Old Coin", "hall");
    EXPECT_EQ(command("take old coin"), "You take the Old Coin.\n");
    EXPECT_EQ(command("take old coin"), "You take the Old Coin.\n");
    EXPECT_EQ(command("take old coin"), "You already have that.\n");
}

TEST_F(NounTest, SomeoneElsewhereIsNotACommandToTalk) {
    add_npc("mira", "Mira", "garden");
    (void)command("talk keeper");
    EXPECT_TRUE(game_->dispatch_player("talk mira").npc_turn.has_value());
}

// An id nobody sees may look like another thing's name.
TEST_F(NounTest, ANameBeatsAnId) {
    add_npc("guard", "Guard Captain", "hall");
    add_npc("guard_2", "Guard", "hall");
    for (const std::string line : {"talk guard", "talk to the Guard"}) {
        EXPECT_EQ(command(line), "You begin talking with Guard.\n") << line;
        (void)command("bye");
    }
    EXPECT_EQ(command("talk guar"), "Which do you mean: Guard Captain or Guard?\n");
    EXPECT_EQ(command("talk captain"), "You begin talking with Guard Captain.\n");
    (void)command("bye");
    EXPECT_EQ(command("talk guard_2"), "You begin talking with Guard.\n");
}

TEST_F(NounTest, AnArticleCanTellNamesApart) {
    add_item("first", "The Lantern", "hall");
    add_item("second", "Lantern", "hall");
    EXPECT_EQ(command("take lantern"), "You take the Lantern.\n");
    EXPECT_EQ(command("drop lantern"), "You drop the Lantern.\n");
    EXPECT_EQ(command("take the lantern"), "You take the The Lantern.\n");
}

TEST_F(NounTest, TalkFindsWhoIsHereFirst) {
    add_npc("apprentice", "Young Keeper", "garden");
    add_npc("beadle", "Beadle", "hall");
    EXPECT_EQ(command("talk to the keeper"), "You begin talking with Keeper.\n");
    (void)command("bye");
    EXPECT_EQ(command("talk young"), "Young Keeper isn't here.\n");
    EXPECT_EQ(command("talk with bea"), "You begin talking with Beadle.\n");
    (void)command("bye");
    EXPECT_EQ(command("talk ee"), "You begin talking with Keeper.\n");
    (void)command("bye");
    EXPECT_EQ(command("talk e"), "Which do you mean: Beadle or Keeper?\n");
}

TEST_F(NounTest, AnExitGoesByItsDirectionOrItsDestination) {
    world_.locations.at("hall").locked_exits.clear();
    world_.locations.at("hall").exits["Spiral Stairs"] = "garden";
    for (const std::string line :
         {"go north", "go spiral stairs", "go to the garden", "go GARDEN"}) {
        EXPECT_NE(command(line).find("Garden\nAn overgrown garden."), std::string::npos) << line;
        (void)command("go south");
    }
    EXPECT_EQ(command("go garden shed"), "You can't go that way.\n");
}

TEST_F(NounTest, AnArticleInAPlaceNameDoesNotMatter) {
    world_.locations.at("hall").locked_exits.clear();
    world_.locations.at("garden").name = "The Walled Garden";
    for (const std::string line : {"go walled garden", "go to the walled garden"}) {
        EXPECT_NE(command(line).find("An overgrown garden."), std::string::npos) << line;
        (void)command("go south");
    }
}

TEST_F(NounTest, TheFirstOpenWayIsTaken) {
    // Both lead to the garden, and the first is locked.
    world_.locations.at("hall").exits["trellis"] = "garden";
    EXPECT_EQ(command("go north"), "That way is locked.\n");
    EXPECT_NE(command("go garden").find("An overgrown garden."), std::string::npos);
}

TEST_F(NounTest, ADirectionBeatsADestination) {
    LocationData shed;
    shed.name = "North";
    world_.locations["shed"] = shed;
    world_.locations.at("hall").exits["east"] = "shed";
    EXPECT_EQ(command("go north"), "That way is locked.\n");
}

TEST_F(NounTest, ADirectionMayHoldAPreposition) {
    world_.locations.at("hall").exits["to the cellar"] = "garden";
    EXPECT_NE(command("go to the cellar").find("An overgrown garden."), std::string::npos);
}

TEST_F(NounTest, WordsAfterADirectionDoNotMatter) {
    world_.locations.at("hall").locked_exits.clear();
    EXPECT_NE(command("go north now please").find("An overgrown garden."), std::string::npos);
    (void)command("go south");
    // To someone, it is speech.
    (void)command("talk keeper");
    EXPECT_TRUE(game_->dispatch_player("go north, young man").npc_turn.has_value());
}

TEST_F(NounTest, AKeyOpensTheWayItIsFor) {
    for (const std::string target : {"north", "garden", "the Garden"}) {
        game_.reset();
        EXPECT_EQ(command("take gate key"), "You take the Gate Key.\n");
        EXPECT_EQ(command("use gate key on " + target),
                  "You use the Gate Key. The way is unlocked.\n");
        EXPECT_EQ(command("use the gate key with " + target), "It isn't locked.\n");
        EXPECT_NE(command("go north").find("Garden"), std::string::npos);
    }
}

TEST_F(NounTest, AKeyMayBeForADirection) {
    world_.items.at("gate_key").unlock_target = "North";
    (void)command("take gate key");
    EXPECT_EQ(command("use gate key on garden"), "You use the Gate Key. The way is unlocked.\n");
}

TEST_F(NounTest, AKeyDoesNothingElsewhere) {
    world_.locations.at("hall").exits["west"] = "hall";
    (void)command("take gate key");
    EXPECT_EQ(command("use gate key on west"), "That doesn't work.\n");
    EXPECT_EQ(command("use gate key on statue"), "That doesn't work.\n");
    EXPECT_EQ(command("go north"), "That way is locked.\n");
}

TEST_F(GameTest, SpokenLinesThatStartWithACommandWordReachTheNpc) {
    (void)command("talk keeper");
    for (const std::string line :
         {"Quit lying to me.", "Help me find the thief.", "Look at me when I talk to you.",
          "Save yourself!", "Load of nonsense.", "Take care of her.", "Go away!", "Go west.",
          "Use your head.", "Talk to me.", "Drop the act.", "Examine your conscience.",
          "Leave me alone.", "Bye the way, who are you?", "Inventory the cellar for me.",
          "take moonbeam", "help!"}) {
        const auto dispatch = game_.dispatch_player(line);
        EXPECT_TRUE(dispatch.events.empty()) << line;
        ASSERT_TRUE(dispatch.npc_turn.has_value()) << line;
        EXPECT_EQ(dispatch.npc_turn->player_text, line);
    }
    EXPECT_EQ(game_.phase(), GamePhase::in_conversation);
}

TEST_F(GameTest, WholeLineCommandsStillWorkInConversation) {
    (void)command("talk keeper");
    EXPECT_NE(joined(command("look")).find("Hall"), std::string::npos);
    EXPECT_NE(joined(command("INVENTORY")).find("carrying nothing"), std::string::npos);
    EXPECT_NE(joined(command("help")).find("In conversation"), std::string::npos);
    EXPECT_NE(joined(command("save 2")).find("Saved to slot 2"), std::string::npos);
    EXPECT_NE(joined(command("load  2")).find("Loaded slot 2"), std::string::npos);
    EXPECT_EQ(game_.phase(), GamePhase::in_conversation);
    for (const std::string line : {"bye", "Goodbye!", "Leave.", "exit conversation"}) {
        EXPECT_NE(joined(command(line)).find("You end the conversation"), std::string::npos)
            << line;
        (void)command("talk keeper");
    }
    (void)command("quit");
    EXPECT_EQ(game_.phase(), GamePhase::game_over);
}

TEST_F(GameTest, CommandsAboutThingsHereAskToFinishTheConversation) {
    (void)command("take old coin");
    (void)command("talk keeper");
    for (const std::string line :
         {"go north", "go to the garden", "take gate key", "take the Gate Key", "take key",
          "take statue", "examine statue", "examine coin", "drop old coin", "drop coin",
          "use old coin on north", "use coin on north", "talk keeper", "talk to the keeper"}) {
        EXPECT_NE(joined(command(line)).find("Finish the conversation"), std::string::npos) << line;
    }
    // Already carried, hidden, not carried, or only part of a word: nothing here is meant.
    for (const std::string line :
         {"take old coin", "take hidden note", "drop gate key", "use gate key on north",
          "examine your conscience", "take stat", "go west", "talk to me"}) {
        EXPECT_TRUE(game_.dispatch_player(line).npc_turn.has_value()) << line;
    }
}

TEST_F(GameTest, TalkToAbsentNpc) {
    (void)game_.submit_npc_tool("keeper", tools::MoveSelf{.location_id = "garden"});
    EXPECT_NE(joined(command("talk keeper")).find("isn't here"), std::string::npos);
    EXPECT_NE(joined(command("talk nobody")).find("Talk to whom?"), std::string::npos);
}

TEST_F(GameTest, VerbAliasesExpand) {
    WorldState world = ct::make_test_world();
    world.config.verb_aliases["grab"] = "take";
    CartridgeGame game(std::move(world), saves_.path());
    (void)game.dispatch_player("grab old coin");
    EXPECT_TRUE(game.world().item_positions.at("old_coin").is_player());
}

TEST_F(GameTest, AnAliasIsForCommandsAndNotForSpeech) {
    WorldState world = ct::make_test_world();
    world.config.verb_aliases["grab"] = "take";
    CartridgeGame game(std::move(world), saves_.path());
    (void)game.dispatch_player("talk keeper");
    const auto dispatch = game.dispatch_player("Grab a chair and sit.");
    ASSERT_TRUE(dispatch.npc_turn.has_value());
    EXPECT_EQ(dispatch.npc_turn->player_text, "Grab a chair and sit.");
    EXPECT_NE(joined(game.dispatch_player("grab old coin").events).find("Finish the conversation"),
              std::string::npos);
}

TEST_F(GameTest, AnAliasNeverTakesAnExitPhraseApart) {
    WorldState world = ct::make_test_world();
    world.config.verb_aliases["exit"] = "quit";
    CartridgeGame game(std::move(world), saves_.path());
    (void)game.dispatch_player("talk keeper");
    EXPECT_NE(
        joined(game.dispatch_player("Exit conversation.").events).find("You end the conversation"),
        std::string::npos);
    (void)game.dispatch_player("exit");
    EXPECT_EQ(game.phase(), GamePhase::game_over);
}

TEST_F(GameTest, AnAliasNeverReplacesABuiltInCommand) {
    WorldState world = ct::make_test_world();
    world.config.verb_aliases = {{"quit", "look"}, {"look", "inventory"}, {"bye", "take"}};
    CartridgeGame game(std::move(world), saves_.path());
    EXPECT_NE(joined(game.dispatch_player("look").events).find("A dusty hall."), std::string::npos);
    (void)game.dispatch_player("talk keeper");
    EXPECT_NE(joined(game.dispatch_player("bye").events).find("You end the conversation"),
              std::string::npos);
    (void)game.dispatch_player("quit");
    EXPECT_EQ(game.phase(), GamePhase::game_over);
}

TEST_F(GameTest, UnknownCommandEchoes) {
    EXPECT_NE(joined(command("dance")).find("Unknown command: dance"), std::string::npos);
}

TEST_F(GameTest, QuitEndsGame) {
    const auto events = command("quit");
    EXPECT_EQ(events.front().kind, EventKind::system);
    EXPECT_EQ(game_.phase(), GamePhase::game_over);
    EXPECT_NE(joined(command("look")).find("scenario has ended"), std::string::npos);
}

TEST_F(GameTest, SignificantTurnsAdvanceClock) {
    EXPECT_EQ(game_.world().clock.turns_elapsed, 0);
    (void)command("look"); // not significant
    (void)game_.after_turn();
    EXPECT_EQ(game_.world().clock.turns_elapsed, 0);
    (void)command("take old coin"); // significant
    (void)game_.after_turn();
    EXPECT_EQ(game_.world().clock.turns_elapsed, 1);
}

TEST_F(GameTest, TimeExpiryEndsGame) {
    // turns_per_period=2, total_periods=3 -> expires at 6 elapsed turns.
    WorldState world = ct::make_test_world();
    world.clock.turns_elapsed = 5;
    CartridgeGame game(std::move(world), saves_.path());
    (void)game.dispatch_player("take old coin");
    const auto events = game.after_turn();
    EXPECT_EQ(game.phase(), GamePhase::game_over);
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back().kind, EventKind::ending);
    EXPECT_NE(events.back().text.find("Time has run out"), std::string::npos);
}

TEST_F(GameTest, RuntimeCheckpointRestoresWorldPhaseAndPendingClockAdvance) {
    auto checkpoint = game_.checkpoint_runtime();
    ASSERT_TRUE(game_.submit_world_action(actions::RelocateItem{
        .item_id = "old_coin",
        .destination = ItemPosition{.holder = ItemHolder::player, .id = {}},
    }));
    ASSERT_TRUE(game_.submit_world_action(actions::BeginConversation{.npc_id = "keeper"}));

    ASSERT_TRUE(game_.restore_runtime(std::move(checkpoint)));
    EXPECT_EQ(game_.phase(), GamePhase::playing);
    EXPECT_FALSE(game_.active_npc_id());
    EXPECT_TRUE(item_is_at(game_.world(), "old_coin", ItemHolder::location, "hall"));
    EXPECT_TRUE(game_.after_turn().empty());
    EXPECT_EQ(game_.world().clock.turns_elapsed, 0);
}

TEST_F(GameTest, SaveAndLoadRoundTrip) {
    (void)command("take old coin");
    (void)command("talk keeper");
    EXPECT_NE(joined(command("save 3")).find("Saved to slot 3"), std::string::npos);

    // Mutate further, then load the snapshot back.
    (void)command("bye");
    (void)command("drop old coin");
    const auto loaded = joined(command("load 3"));
    EXPECT_NE(loaded.find("Loaded slot 3"), std::string::npos);
    EXPECT_EQ(game_.phase(), GamePhase::in_conversation);
    EXPECT_EQ(game_.active_npc_id(), "keeper");
    EXPECT_TRUE(game_.world().item_positions.at("old_coin").is_player());
}

TEST_F(GameTest, FailedConversationRestoreRollsBackEntireLoad) {
    game_.set_conversation_hooks(
        []() -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{{"keeper", nlohmann::json::object()}};
        },
        [](const nlohmann::json &) -> std::expected<void, std::string> {
            return std::unexpected("conversation rejected");
        });
    (void)command("take old coin");
    ASSERT_NE(joined(command("save 4")).find("Saved to slot 4"), std::string::npos);
    (void)command("drop old coin");
    ASSERT_TRUE(game_.world().item_positions.at("old_coin").is_location("hall"));

    const auto loaded = joined(command("load 4"));
    EXPECT_NE(loaded.find("conversation rejected"), std::string::npos);
    EXPECT_TRUE(game_.world().item_positions.at("old_coin").is_location("hall"));
    EXPECT_EQ(game_.phase(), GamePhase::playing);
}

TEST_F(GameTest, LoadMissingSlot) {
    EXPECT_NE(joined(command("load 7")).find("No save in slot 7"), std::string::npos);
}

TEST_F(GameTest, RunawayRememberingIsRefusedAndTheGameStillSaves) {
    MemoryEntry memory;
    memory.summary = "Again.";
    actions::ActionOutcome outcome;
    for (std::size_t count = 0; count <= MAX_NPC_MEMORIES; ++count) {
        ASSERT_TRUE(outcome.has_value()) << count << ": " << outcome.error().reason;
        outcome =
            game_.submit_world_action(actions::AddMemory{.npc_id = "keeper", .memory = memory});
    }
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error().reason, "NPC memory is full");
    EXPECT_TRUE(game_.restore_runtime(game_.checkpoint_runtime()).has_value());
    EXPECT_TRUE(game_.save(1).has_value());
}

TEST_F(GameTest, SaveAndLoadRejectMalformedSlots) {
    EXPECT_NE(joined(command("save nope")).find("must be an integer"), std::string::npos);
    EXPECT_NE(joined(command("load 1 extra")).find("Usage: load"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(saves_.path() / "slot_1.json"));
}

// --- the broken_wheel example can be won and lost --------------------------------------

class BrokenWheelTest : public ::testing::Test {
  protected:
    BrokenWheelTest() : saves_("wheelsaves"), game_(ct::broken_wheel_example(), saves_.path()) {}

    std::string play(const std::string &line) {
        auto events = game_.dispatch_player(line).events;
        const auto after = game_.after_turn();
        events.insert(events.end(), after.begin(), after.end());
        return joined(events);
    }

    ct::TempDir saves_;
    CartridgeGame game_;
};

TEST_F(BrokenWheelTest, BringingTheManifestToTheMarketWins) {
    (void)play("talk marcus");
    ASSERT_TRUE(game_.submit_npc_tool("marcus", tools::GiveItem{.item_id = "cargo_manifest"}));
    (void)play("bye");
    const auto ending = play("go north");
    EXPECT_EQ(game_.phase(), GamePhase::game_over);
    EXPECT_NE(ending.find("riders after Harlan"), std::string::npos) << ending;
}

TEST_F(BrokenWheelTest, TheTrailGoesColdAtSundown) {
    std::string last;
    for (int turn = 0; turn < 10; ++turn) {
        ASSERT_EQ(game_.phase(), GamePhase::playing) << turn;
        last = play(turn % 2 == 0 ? "go north" : "go south");
    }
    EXPECT_EQ(game_.phase(), GamePhase::game_over);
    EXPECT_NE(last.find("trail goes cold"), std::string::npos) << last;
}

TEST(GameConstruction, UnsafeCartridgeIdCannotSelectASavePath) {
    WorldState world = ct::make_test_world();
    world.manifest.id = "../escape";
    ct::TempDir saves("unsafeidsaves");
    EXPECT_THROW((void)CartridgeGame(std::move(world), saves.path()), std::invalid_argument);
}

} // namespace
} // namespace chronicle
