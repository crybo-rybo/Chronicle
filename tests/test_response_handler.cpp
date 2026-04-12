#include "ai/response_handler.hpp"
#include "ai/tool_registry.hpp"
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

TEST(ResponseHandlerTest, NarrateMutations) {
    MockRenderer renderer;
    TokenQueue queue;
    World world;
    
    Item sword;
    sword.id = "sword";
    sword.name = "Iron Sword";
    world.items["sword"] = sword;

    ResponseHandler handler(renderer, queue, world);

    std::vector<MutationRequest> mutations;
    mutations.push_back({MutationRequest::Type::GiveItemToPlayer, "marcus", {{"item_id", "sword"}}});
    mutations.push_back({MutationRequest::Type::UpdateNpcMood, "marcus", {{"mood", "angry"}}});
    mutations.push_back({MutationRequest::Type::SetFlag, "marcus", {{"flag_id", "test"}}}); // silent

    handler.narrate_mutations(mutations, "Marcus");

    ASSERT_EQ(renderer.actions.size(), 2);
    EXPECT_EQ(renderer.actions[0], "Marcus hands you the Iron Sword.");
    EXPECT_EQ(renderer.actions[1], "Marcus's expression shifts — they seem angry now.");
}
