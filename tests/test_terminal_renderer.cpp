#include "rendering/terminal_renderer.hpp"
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace {

chronicle::World make_test_world() {
    chronicle::World w;
    chronicle::Location loc;
    loc.id = "tavern";
    loc.name = "The Broken Wheel Tavern";
    loc.base_description = "A smoky common room with a crackling fire.";
    loc.exits = {{"north", "market_square"}};
    loc.items = {"crumpled_note"};
    loc.npcs = {"marcus"};
    w.locations["tavern"] = loc;

    chronicle::Item note;
    note.id = "crumpled_note";
    note.name = "crumpled note";
    note.description = "A hastily scrawled message.";
    w.items["crumpled_note"] = note;

    chronicle::Npc marcus;
    marcus.identity.id = "marcus";
    marcus.identity.name = "Marcus";
    marcus.state.mood = "neutral";
    w.npcs["marcus"] = marcus;

    w.player.current_location = "tavern";
    return w;
}

} // namespace

TEST(TerminalRendererTest, RenderSceneContainsLocationName) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);
    auto world = make_test_world();
    const auto &loc = world.locations.at("tavern");

    renderer.render_scene(loc, world);
    EXPECT_NE(out.str().find("Broken Wheel Tavern"), std::string::npos);
}

TEST(TerminalRendererTest, RenderSceneContainsDescription) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);
    auto world = make_test_world();
    const auto &loc = world.locations.at("tavern");

    renderer.render_scene(loc, world);
    EXPECT_NE(out.str().find("smoky common room"), std::string::npos);
}

TEST(TerminalRendererTest, RenderSceneListsExits) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);
    auto world = make_test_world();
    const auto &loc = world.locations.at("tavern");

    renderer.render_scene(loc, world);
    EXPECT_NE(out.str().find("north"), std::string::npos);
}

TEST(TerminalRendererTest, RenderSceneListsItems) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);
    auto world = make_test_world();
    const auto &loc = world.locations.at("tavern");

    renderer.render_scene(loc, world);
    EXPECT_NE(out.str().find("crumpled note"), std::string::npos);
}

TEST(TerminalRendererTest, RenderSceneListsNpcs) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);
    auto world = make_test_world();
    const auto &loc = world.locations.at("tavern");

    renderer.render_scene(loc, world);
    EXPECT_NE(out.str().find("Marcus"), std::string::npos);
}

TEST(TerminalRendererTest, RenderMoveContainsDirection) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    renderer.render_move("north", "Market Square");
    EXPECT_NE(out.str().find("north"), std::string::npos);
    EXPECT_NE(out.str().find("Market Square"), std::string::npos);
}

TEST(TerminalRendererTest, StreamTokenOutputsText) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    renderer.stream_token("hello");
    EXPECT_EQ(out.str(), "hello");
}

TEST(TerminalRendererTest, BeginNpcDialogueColorsStreamedTokensUntilFlush) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer({"Marcus"}, out, in, true);

    renderer.begin_npc_dialogue("Marcus");
    renderer.stream_token("hello");
    renderer.flush_dialogue();

    auto result = out.str();
    EXPECT_NE(result.find("\033["), std::string::npos);
    EXPECT_LT(result.find("\033["), result.find("hello"));
    EXPECT_NE(result.find("hello\033[0m\n"), std::string::npos);
}

TEST(TerminalRendererTest, FlushDialogueAddsNewline) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    renderer.flush_dialogue();
    std::string result = out.str();
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.back(), '\n');
}

TEST(TerminalRendererTest, RenderActionContainsNarration) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    renderer.render_action("Marcus slides a note");
    EXPECT_NE(out.str().find("Marcus slides a note"), std::string::npos);
}

TEST(TerminalRendererTest, RenderInventoryWithItems) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);
    auto world = make_test_world();
    world.player.inventory = {"crumpled_note"};

    renderer.render_inventory(world.player, world);
    EXPECT_NE(out.str().find("crumpled note"), std::string::npos);
}

TEST(TerminalRendererTest, RenderInventoryEmpty) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);
    auto world = make_test_world();
    world.player.inventory = {};

    renderer.render_inventory(world.player, world);
    EXPECT_NE(out.str().find("nothing"), std::string::npos);
}

TEST(TerminalRendererTest, RenderItemExamine) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);
    auto world = make_test_world();
    const auto &note = world.items.at("crumpled_note");

    renderer.render_item_examine(note);
    EXPECT_NE(out.str().find("crumpled note"), std::string::npos);
    EXPECT_NE(out.str().find("A hastily scrawled message."), std::string::npos);
}

TEST(TerminalRendererTest, RenderErrorContainsMessage) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    renderer.render_error("bad input");
    EXPECT_NE(out.str().find("bad input"), std::string::npos);
}

TEST(TerminalRendererTest, RenderSystemContainsMessage) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    renderer.render_system("Game saved");
    EXPECT_NE(out.str().find("Game saved"), std::string::npos);
}

TEST(TerminalRendererTest, RenderTimeAdvance) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    chronicle::Clock clock;
    clock.day = 2;
    clock.period = chronicle::TimePeriod::Morning;

    renderer.render_time_advance(clock);
    EXPECT_NE(out.str().find("Morning"), std::string::npos);
    EXPECT_NE(out.str().find("Day 2"), std::string::npos);
}

TEST(TerminalRendererTest, RenderResolutionContainsNarration) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    renderer.render_resolution("The story has ended.");
    EXPECT_NE(out.str().find("The story has ended."), std::string::npos);
}

TEST(TerminalRendererTest, GetPlayerInputReturnsLine) {
    std::ostringstream out;
    std::istringstream in("hello world\n");
    chronicle::TerminalRenderer renderer(out, in, false);

    std::string result = renderer.get_player_input("> ");
    EXPECT_EQ(result, "hello world");
}

TEST(TerminalRendererTest, RenderNpcIntroContainsName) {
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, false);

    renderer.render_npc_intro("Marcus", "neutral");
    EXPECT_NE(out.str().find("Marcus"), std::string::npos);
}

TEST(TerminalRendererTest, AssignNpcColorsDeterministicOrder) {
    // Two renderers given the same NPC IDs in different order
    // should produce the same color assignments.
    std::ostringstream out1, out2;
    std::istringstream in1, in2;
    chronicle::TerminalRenderer r1(out1, in1, true);
    chronicle::TerminalRenderer r2(out2, in2, true);

    r1.assign_npc_colors({"zara", "marcus", "elara"});
    r2.assign_npc_colors({"marcus", "elara", "zara"});

    // Render NPC intros in the same order and compare output
    for (const auto &name : {"elara", "marcus", "zara"}) {
        r1.render_npc_intro(name, "neutral");
        r2.render_npc_intro(name, "neutral");
    }
    EXPECT_EQ(out1.str(), out2.str());
}

TEST(TerminalRendererTest, ConstructorAssignsNpcColorsDeterministically) {
    std::ostringstream out1, out2;
    std::istringstream in1, in2;
    chronicle::TerminalRenderer r1({"zara", "marcus", "elara"}, out1, in1, true);
    chronicle::TerminalRenderer r2({"marcus", "elara", "zara"}, out2, in2, true);

    for (const auto &name : {"elara", "marcus", "zara"}) {
        r1.render_npc_intro(name, "neutral");
        r2.render_npc_intro(name, "neutral");
    }
    EXPECT_EQ(out1.str(), out2.str());
}

TEST(TerminalRendererTest, AssignNpcColorsPreservesLazyFallback) {
    // Pre-assign some NPCs, then encounter a new one via lazy path.
    std::ostringstream out;
    std::istringstream in;
    chronicle::TerminalRenderer renderer(out, in, true);

    renderer.assign_npc_colors({"marcus", "elara"});

    // "zara" was not pre-assigned, but should still get a color via lazy path
    renderer.render_npc_intro("zara", "calm");
    EXPECT_NE(out.str().find("zara"), std::string::npos);
}

TEST(TerminalRendererTest, AssignNpcColorsSkipsDuplicates) {
    // Calling assign_npc_colors with a name already in the map should not
    // change its index.
    std::ostringstream out1, out2;
    std::istringstream in1, in2;
    chronicle::TerminalRenderer r1(out1, in1, true);
    chronicle::TerminalRenderer r2(out2, in2, true);

    r1.assign_npc_colors({"marcus"});
    // Trigger lazy for marcus on r1 via intro, then assign again
    r1.render_npc_intro("marcus", "neutral");

    r2.assign_npc_colors({"marcus"});
    r2.assign_npc_colors({"marcus"}); // duplicate call
    r2.render_npc_intro("marcus", "neutral");

    EXPECT_EQ(out1.str(), out2.str());
}
