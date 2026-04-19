#include "ai/response_handler.hpp"
#include "engine/token_queue.hpp"
#include "entities/world.hpp"
#include "rendering/renderer.hpp"
#include <gtest/gtest.h>

using namespace chronicle;

class MockRenderer : public Renderer {
  public:
    std::vector<std::string> actions;
    std::vector<std::string> tokens;

    void render_scene(const Location &, const World &) override {}
    void render_move(const std::string &, const std::string &) override {}
    void render_npc_intro(std::string_view, std::string_view) override {}
    void begin_npc_dialogue(std::string_view) override {}
    
    void stream_token(std::string_view token) override {
        tokens.push_back(std::string(token));
    }
    
    void flush_dialogue() override {}
    
    void render_action(std::string_view narration) override {
        actions.push_back(std::string(narration));
    }
    
    void render_inventory(const Player &, const World &) override {}
    void render_item_examine(const Item &) override {}
    void render_error(std::string_view) override {}
    void render_system(std::string_view) override {}
    void render_time_advance(const Clock &) override {}
    void render_resolution(std::string_view) override {}
    std::string get_player_input(std::string_view) override { return ""; }
    void clear_input_line() override {}
};

TEST(ResponseHandlerTest, OnTokenPushesToQueue) {
    MockRenderer renderer;
    TokenQueue queue;
    World world;
    ResponseHandler handler(renderer, queue, world);

    handler.on_token("hello");
    handler.on_token(" ");
    handler.on_token("world");

    EXPECT_EQ(queue.try_pop(), "hello");
    EXPECT_EQ(queue.try_pop(), " ");
    EXPECT_EQ(queue.try_pop(), "world");
}

TEST(ResponseHandlerTest, NarrateMutationsUsesTemplatesForVisibleAndSilentMutations) {
    MockRenderer renderer;
    TokenQueue queue;
    World world;
    
    Item sword;
    sword.id = "sword";
    sword.name = "Iron Sword";
    world.items["sword"] = sword;

    std::unordered_map<std::string, std::string> templates = {
        {"give_item_to_player", "{npc} passes over {item}."},
        {"take_item_from_player", "{npc} accepts {item}."},
        {"update_npc_mood", "{npc} now seems {mood}."},
        {"move_npc", "{npc} leaves for now."},
        {"reveal_knowledge", ""},
        {"update_npc_trust", ""},
        {"add_memory", ""},
        {"set_flag", ""},
    };

    ResponseHandler handler(renderer, queue, world, templates);

    using Source = MutationRequest::Source;
    std::vector<MutationRequest> mutations;
    mutations.push_back({MutationRequest::Type::GiveItemToPlayer, Source::Npc, "marcus", {{"item_id", "sword"}}});
    mutations.push_back({MutationRequest::Type::TakeItemFromPlayer, Source::Npc, "marcus", {{"item_id", "sword"}}});
    mutations.push_back({MutationRequest::Type::UpdateNpcMood, Source::Npc, "marcus", {{"mood", "angry"}}});
    mutations.push_back({MutationRequest::Type::MoveNpc, Source::Npc, "marcus", {{"location_id", "market"}}});
    mutations.push_back({MutationRequest::Type::RevealKnowledge, Source::Npc, "marcus", {{"fact_id", "test"}}});
    mutations.push_back({MutationRequest::Type::UpdateNpcTrust, Source::Npc, "marcus", {{"delta", "5"}}});
    mutations.push_back({MutationRequest::Type::AddMemory, Source::Npc, "marcus", {{"summary", "test"}}});
    mutations.push_back({MutationRequest::Type::SetFlag, Source::Npc, "", {{"flag_id", "test"}}});

    handler.narrate_mutations(mutations, "Marcus");

    ASSERT_EQ(renderer.actions.size(), 4);
    EXPECT_EQ(renderer.actions[0], "Marcus passes over Iron Sword.");
    EXPECT_EQ(renderer.actions[1], "Marcus accepts Iron Sword.");
    EXPECT_EQ(renderer.actions[2], "Marcus now seems angry.");
    EXPECT_EQ(renderer.actions[3], "Marcus leaves for now.");
}
