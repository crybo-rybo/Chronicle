// NPC sessions against scry's scripted loopback server: the shipping harness,
// HTTP, and tool dispatch run for real, and only the model is scripted.
#include <algorithm>
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <scry/testing/scripted_server.hpp>
#include <scry/testing/streams.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "chronicle/llm/npc_sessions.hpp"
#include "chronicle/prompt.hpp"
#include "chronicle/runtime.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;
namespace st = scry::testing;
using nlohmann::json;

scry::testing::ScriptedServer start_server() {
    auto server = st::ScriptedServer::create();
    if (!server) {
        throw std::runtime_error("scripted server: " + server.error().message);
    }
    return std::move(*server);
}

// A world whose history budget never trims, so tests opt in to trimming.
WorldState untrimmed_world() {
    WorldState world = ct::make_test_world();
    world.config.max_history_tokens = 1'000'000;
    return world;
}

json body_of(const st::CapturedRequest &request) {
    return json::parse(request.body);
}

// Every tool result the request hands back to the model, in order.
std::vector<json> tool_results(const st::CapturedRequest &request) {
    std::vector<json> results;
    for (const auto &message : body_of(request)["messages"]) {
        if (message["role"] == "tool") {
            results.push_back(json::parse(message["content"].get<std::string>()));
        }
    }
    return results;
}

std::vector<std::string> offered_tools(const st::CapturedRequest &request) {
    std::vector<std::string> names;
    for (const auto &tool : body_of(request)["tools"]) {
        names.push_back(tool["function"]["name"].get<std::string>());
    }
    return names;
}

std::string joined(const GameEvents &events) {
    std::string all;
    for (const auto &event : events) {
        all += event.text + "\n";
    }
    return all;
}

class NpcSessionTest : public ::testing::Test {
  protected:
    NpcSessionTest() : NpcSessionTest(untrimmed_world()) {}
    explicit NpcSessionTest(WorldState world)
        : server_(start_server()), saves_("sessionsaves"), game_(std::move(world), saves_.path()) {
        (void)game_.dispatch_player("talk keeper");
    }

    [[nodiscard]] EndpointConfig endpoint() const {
        return {.base_url = server_.url(), .model = "scripted", .timeout_ms = 10'000};
    }

    void reply(const std::string_view text) {
        server_.enqueue({.body_chunks = {st::openai_text_stream(text)}});
    }
    void call(const std::initializer_list<st::ToolUseBlock> calls) {
        server_.enqueue({.body_chunks = {st::openai_tool_stream(calls)}});
    }
    void refuse(const int status) {
        server_.enqueue(
            {.status = status, .body_chunks = {st::openai_error_body(status, "scripted")}});
    }

    [[nodiscard]] json world_json() const { return game_.world(); }
    [[nodiscard]] st::CapturedRequest last_request() const { return server_.requests().back(); }

    st::ScriptedServer server_;
    ct::TempDir saves_;
    CartridgeGame game_;
};

// --- tool calls and the gate -------------------------------------------------

TEST_F(NpcSessionTest, ToolCallsReachTheGateAndAreAcknowledged) {
    NpcSessionManager sessions(game_, endpoint());
    call({{.id = "c1", .name = "say", .arguments = R"({"text":"Mind the gate."})"},
          {.id = "c2", .name = "set_flag", .arguments = R"({"flag_id":"gate_seen","value":true})"},
          {.id = "c3", .name = "inspect_item", .arguments = R"({"item_id":"old_coin"})"}});
    reply("(the keeper nods)");

    const auto turn = sessions.run_turn("keeper", "What about the gate?");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_TRUE(game_.world().flags.at("gate_seen"));
    // Dialogue came from the say tool, so the closing text is not repeated.
    ASSERT_EQ(turn->front().kind, EventKind::dialogue);
    EXPECT_EQ(turn->front().text, "Keeper: \"Mind the gate.\"");
    EXPECT_EQ(std::ranges::count(*turn, EventKind::dialogue, &GameEvent::kind), 1);

    ASSERT_EQ(server_.requests().size(), 2U);
    const std::vector<json> expected = {
        {{"ok", true},
         {"result", "The player heard you. Finish acting if you must, then end your turn without "
                    "speaking again."},
         {"error", ""}},
        {{"ok", true}, {"result", "ok"}, {"error", ""}},
        {{"ok", true}, {"result", "Old Coin: A worn coin.\nIt reads: MDCCLX"}, {"error", ""}},
    };
    EXPECT_EQ(tool_results(last_request()), expected);
    // What the NPC reads off an item is not shown to the player.
    EXPECT_EQ(joined(*turn).find("MDCCLX"), std::string::npos);
}

TEST_F(NpcSessionTest, ClosingTextIsDialogueWhenNothingWasSaid) {
    NpcSessionManager sessions(game_, endpoint());
    reply("  The garden is closed.  ");
    const auto turn = sessions.run_turn("keeper", "May I pass?");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    ASSERT_EQ(turn->size(), 1U);
    EXPECT_EQ(turn->front().text, "Keeper: \"The garden is closed.\"");
}

TEST_F(NpcSessionTest, RequestOffersTheReflectedSchemas) {
    NpcSessionManager sessions(game_, endpoint());
    reply("Hello.");
    ASSERT_TRUE(sessions.run_turn("keeper", "hello").has_value());

    const auto body = body_of(last_request());
    const auto schemas = npc_tool_schemas();
    ASSERT_EQ(body["tools"].size(), schemas.size());
    for (const auto &tool : body["tools"]) {
        const auto &function = tool["function"];
        const auto schema = std::ranges::find(schemas, function["name"].get<std::string>(),
                                              &ReflectedToolSchema::name);
        ASSERT_NE(schema, schemas.end());
        EXPECT_EQ(function["description"], schema->description);
        EXPECT_EQ(function["parameters"], json::parse(schema->input_schema));
    }
    EXPECT_EQ(body["messages"][0]["role"], "system");
    EXPECT_EQ(body["messages"][0]["content"], build_npc_system_prompt(game_.world(), "keeper"));
    // A plain turn never forces a tool call.
    EXPECT_FALSE(body.contains("tool_choice"));
}

TEST_F(NpcSessionTest, GateRejectionIsReturnedToTheModelAsData) {
    NpcSessionManager sessions(game_, endpoint());
    call({{.id = "c1", .name = "give_item", .arguments = R"({"item_id":"old_coin"})"}});
    reply("I have nothing for you.");
    const auto before = world_json();

    const auto turn = sessions.run_turn("keeper", "Give me the coin.");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(tool_results(last_request()),
              (std::vector<json>{
                  {{"ok", false}, {"result", ""}, {"error", "NPC does not hold that item"}}}));
    // The player sees only dialogue, and the world is as it was.
    ASSERT_EQ(turn->size(), 1U);
    EXPECT_EQ(turn->front().text, "Keeper: \"I have nothing for you.\"");
    EXPECT_EQ(world_json(), before);
}

TEST_F(NpcSessionTest, MalformedArgumentsAreExplainedAndNeverReachTheGate) {
    NpcSessionManager sessions(game_, endpoint());
    call({{.id = "c1", .name = "update_mood", .arguments = R"({"mood":"angry"})"},
          {.id = "c2", .name = "set_flag", .arguments = R"({"flag_id":"gate_seen","extra":1})"},
          {.id = "c3", .name = "update_trust", .arguments = R"({"delta":40000})"}});
    reply("Hmm.");
    const auto before = world_json();

    ASSERT_TRUE(sessions.run_turn("keeper", "hello").has_value());
    const std::vector<json> expected = {
        {{"error", "$.mood is not a declared enumerator; must be one of: fearful, friendly, "
                   "grieving, hostile, neutral, suspicious"}},
        {{"error", "$ contains unknown member \"extra\"\n$.value is a required member"}},
        {{"error", "$.delta is outside the integer range"}},
    };
    EXPECT_EQ(tool_results(last_request()), expected);
    EXPECT_EQ(world_json(), before);
}

class RestrictedSessionTest : public NpcSessionTest {
  protected:
    static WorldState restricted_world() {
        WorldState world = untrimmed_world();
        world.npcs.at("keeper").identity.tool_policy.allowed_tools = {"say", "remember"};
        return world;
    }
    RestrictedSessionTest() : NpcSessionTest(restricted_world()) {}
};

TEST_F(RestrictedSessionTest, DisallowedToolIsNeitherOfferedNorDispatched) {
    NpcSessionManager sessions(game_, endpoint());
    call({{.id = "c1", .name = "update_trust", .arguments = R"({"delta":50})"}});
    reply("No.");
    const auto before = world_json();

    ASSERT_TRUE(sessions.run_turn("keeper", "Trust me.").has_value());
    EXPECT_EQ(offered_tools(last_request()), (std::vector<std::string>{"say", "remember"}));
    EXPECT_EQ(tool_results(last_request()),
              (std::vector<json>{
                  {{"error", "unknown tool \"update_trust\"; registered tools: remember, say"}}}));
    EXPECT_EQ(world_json(), before);
}

TEST_F(NpcSessionTest, AnthropicDialectDispatchesTheSameTools) {
    auto anthropic = endpoint();
    anthropic.anthropic_dialect = true;
    NpcSessionManager sessions(game_, anthropic);
    server_.enqueue({.body_chunks = {st::anthropic_tool_stream(
                         {{.id = "c1", .name = "update_trust", .arguments = R"({"delta":7})"}})}});
    server_.enqueue({.body_chunks = {st::anthropic_text_stream("Very well.")}});

    const auto turn = sessions.run_turn("keeper", "Please.");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(game_.world().npcs.at("keeper").state.trust_toward_player, 7);
    EXPECT_EQ(last_request().target, "/v1/messages");
}

// --- the turn transaction ----------------------------------------------------

// A turn that changed the world, moved the NPC, and ended the conversation
// before it failed.
class FailedTurnTest : public NpcSessionTest {
  protected:
    void mutate_everything() {
        call({{.id = "c1",
               .name = "set_flag",
               .arguments = R"({"flag_id":"gate_seen","value":true})"},
              {.id = "c2", .name = "give_item", .arguments = R"({"item_id":"keepsake"})"},
              {.id = "c3", .name = "update_trust", .arguments = R"({"delta":60})"},
              {.id = "c4", .name = "say", .arguments = R"({"text":"Farewell."})"},
              {.id = "c5", .name = "move_self", .arguments = R"({"location_id":"garden"})"}});
    }

    // Runs a turn that must fail and checks that the world, the phase, and the
    // history are what the last good turn committed. Then proves the session
    // still works and never mentions the failed exchange again.
    void expect_rolled_back(NpcSessionManager &sessions, const std::string &reason) {
        const auto world = world_json();
        const auto history = sessions.snapshot_conversations();
        ASSERT_TRUE(history.has_value()) << history.error();

        const auto turn = sessions.run_turn("keeper", "FAILED EXCHANGE");
        ASSERT_FALSE(turn.has_value());
        EXPECT_TRUE(turn.error().world_rolled_back);
        EXPECT_NE(turn.error().message.find(reason), std::string::npos) << turn.error().message;

        EXPECT_EQ(world_json(), world);
        EXPECT_EQ(game_.phase(), GamePhase::in_conversation);
        EXPECT_EQ(game_.active_npc_id(), std::optional<std::string>("keeper"));
        const auto after = sessions.snapshot_conversations();
        ASSERT_TRUE(after.has_value()) << after.error();
        EXPECT_EQ(*after, *history);

        reply("Where were we?");
        const auto recovered = sessions.run_turn("keeper", "hello again");
        ASSERT_TRUE(recovered.has_value()) << recovered.error().message;
        EXPECT_EQ(last_request().body.find("FAILED EXCHANGE"), std::string::npos);
        EXPECT_EQ(last_request().body.find("Farewell."), std::string::npos);
    }

    std::vector<std::string> ids_;

    void commit_good_turn(NpcSessionManager &sessions) {
        call({{.id = "g1", .name = "update_trust", .arguments = R"({"delta":5})"}});
        reply("Welcome.");
        const auto turn = sessions.run_turn("keeper", "hello");
        ASSERT_TRUE(turn.has_value()) << turn.error().message;
        ASSERT_EQ(game_.world().npcs.at("keeper").state.trust_toward_player, 5);
    }
};

TEST_F(FailedTurnTest, ProviderRefusalAfterToolsRollsEverythingBack) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    mutate_everything();
    refuse(400);
    expect_rolled_back(sessions, "provider rejected the request");
}

TEST_F(FailedTurnTest, AProviderThatFailsInsideAResponseRollsBack) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    mutate_everything();
    server_.enqueue(
        {.body_chunks = {"data: {\"error\":{\"message\":\"engine died\",\"type\":"
                         "\"InternalServerError\",\"code\":500}}\n\ndata: [DONE]\n\n"}});
    const auto requests = server_.requests().size();
    expect_rolled_back(sessions, "InternalServerError");
    // Two for the failed turn, with no second try, and one for the recovery.
    EXPECT_EQ(server_.requests().size(), requests + 3);
}

// A refusal need not say anything, and then only its status tells it from a response
// the model spoiled.
TEST_F(FailedTurnTest, ARefusalWithoutAWordRollsBack) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    mutate_everything();
    server_.enqueue({.status = 400, .body_chunks = {"{}"}});
    const auto requests = server_.requests().size();
    expect_rolled_back(sessions, "HTTP 400");
    EXPECT_EQ(server_.requests().size(), requests + 3);
}

TEST_F(FailedTurnTest, DroppedConnectionRollsBackWithoutRepeatingTools) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    mutate_everything();
    // Output already arrived when the connection dropped, so nothing is retried.
    server_.enqueue(
        {.body_chunks = {st::openai_text_stream("Half a"), "never sent"}, .close_after_chunks = 1});
    server_.enqueue({.status = 400, .body_chunks = {"{}"}});
    const auto requests = server_.requests().size();
    const auto world = world_json();
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_FALSE(turn.has_value());
    EXPECT_EQ(world_json(), world);
    EXPECT_EQ(server_.requests().size(), requests + 2);
}

TEST_F(FailedTurnTest, TransientFailureIsRetriedWithoutRepeatingTools) {
    NpcSessionManager sessions(game_, endpoint());
    call({{.id = "c1", .name = "update_trust", .arguments = R"({"delta":5})"}});
    refuse(503);
    reply("Welcome.");
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(server_.requests().size(), 3U);
    EXPECT_EQ(game_.world().npcs.at("keeper").state.trust_toward_player, 5);
}

TEST_F(FailedTurnTest, TwoEmptyResponsesRollBack) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    const auto requests = server_.requests().size();
    reply("");
    reply("");
    expect_rolled_back(sessions, "no content");
    EXPECT_EQ(server_.requests().size(), requests + 2 + 1);
}

TEST_F(FailedTurnTest, EmptyResponsesAfterToolsThatShowNothingRollBack) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    for (const std::string id : {"c1", "c2"}) {
        ids_.push_back(id);
        call({{.id = ids_.back(),
               .name = "remember",
               .arguments = R"({"summary":"A silent note."})"}});
        reply("");
    }
    expect_rolled_back(sessions, "no content");
}

TEST_F(FailedTurnTest, AnUnusableResponseIsAskedForAgainFromTheStart) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    // The first try changed the world without showing it.
    call({{.id = "c1", .name = "update_trust", .arguments = R"({"delta":50})"}});
    reply("");
    call({{.id = "c2", .name = "update_trust", .arguments = R"({"delta":2})"}});
    reply("Better.");
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Better.\"\n");
    EXPECT_EQ(game_.world().npcs.at("keeper").state.trust_toward_player, 7);
    EXPECT_EQ(last_request().body.find("\"c1\""), std::string::npos);
}

TEST_F(FailedTurnTest, SilentProviderTimesOutAndRollsBack) {
    auto impatient = endpoint();
    impatient.timeout_ms = 500;
    NpcSessionManager sessions(game_, impatient);
    commit_good_turn(sessions);
    mutate_everything();
    for (int attempt = 0; attempt < 3; ++attempt) {
        server_.enqueue({.body_chunks = {st::openai_text_stream("late")}, .hold = true});
    }
    const auto started = std::chrono::steady_clock::now();
    expect_rolled_back(sessions, "timed out");
    // Three bounded attempts and two sub-second backoffs.
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(10));
}

TEST_F(FailedTurnTest, PlayerTextThatIsNotUtf8IsRefusedBeforeAnyRequest) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    const auto requests = server_.requests().size();
    const auto world = world_json();

    const auto turn = sessions.run_turn("keeper", "caf\xE9");
    ASSERT_FALSE(turn.has_value());
    EXPECT_TRUE(turn.error().world_rolled_back);
    EXPECT_EQ(server_.requests().size(), requests);
    EXPECT_EQ(world_json(), world);

    reply("Pardon?");
    EXPECT_TRUE(sessions.run_turn("keeper", "caf\xC3\xA9").has_value());
}

// Applies each call for real, then throws in place of acknowledging one of them.
class FaultyGate : public NpcSessionManager {
  public:
    using NpcSessionManager::NpcSessionManager;
    std::string faulty_tool;

  protected:
    ToolOutcome submit_tool(const std::string &npc_id, const tools::NpcToolCall &call) override {
        auto outcome = NpcSessionManager::submit_tool(npc_id, call);
        if (tools::tool_name(call) == faulty_tool) {
            throw std::runtime_error("gate fault");
        }
        return outcome;
    }
};

TEST_F(FailedTurnTest, ThrowingGateCancelsTheTurnAndRollsEverythingBack) {
    FaultyGate sessions(game_, endpoint());
    commit_good_turn(sessions);
    const auto requests = server_.requests().size();
    sessions.faulty_tool = "update_trust";
    mutate_everything();
    expect_rolled_back(sessions, "gate fault");
    // The cancelled turn sent nothing back to the model: one request for it, one
    // for the recovery turn.
    EXPECT_EQ(server_.requests().size(), requests + 2);
}

TEST_F(FailedTurnTest, ConsoleShowsStubDialogueAndKeepsPlaying) {
    ConsoleRuntime runtime(game_, endpoint());
    mutate_everything();
    refuse(400);
    const auto world = world_json();
    const auto events = runtime.handle_line("hello");
    ASSERT_GE(events.size(), 2U);
    EXPECT_EQ(events[0].kind, EventKind::warning);
    EXPECT_NE(events[0].text.find("world remained unchanged"), std::string::npos);
    EXPECT_NE(events[1].text.find(STUB_REPLY), std::string::npos);
    EXPECT_EQ(joined(events).find("Farewell."), std::string::npos);
    EXPECT_EQ(world_json(), world);
}

// --- turns that end in a response nobody can use ---------------------------------

// Many models speak through the say tool and then stop with an empty message, or
// run on until a response is cut off. Scry fails such a turn; Chronicle keeps what
// was done before it, history included.
class WordlessTurnTest : public NpcSessionTest {
  protected:
    // Two rounds holding every kind of answer a call can get, then `closing`.
    void script(const std::string_view closing) {
        server_.enqueue(
            {.body_chunks = {st::openai_text_tool_stream(
                 "Let me think.",
                 {{.id = "c1", .name = "say", .arguments = R"({"text":"Take it."})"},
                  {.id = "c2", .name = "summon", .arguments = R"({"n":[1,{"a":null}]})"},
                  {.id = "c3", .name = "give_item", .arguments = R"({"item_id":"old_coin"})"}})}});
        call({{.id = "c4", .name = "update_mood", .arguments = R"({"mood":"angry"})"},
              {.id = "c5", .name = "give_item", .arguments = R"({"item_id":"keepsake"})"}});
        reply(closing);
    }
};

TEST_F(WordlessTurnTest, KeepsWhatItsToolsDidAndGoesOn) {
    NpcSessionManager sessions(game_, endpoint());
    script("");
    const auto turn = sessions.run_turn("keeper", "May I have it?");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Take it.\"\nKeeper hands you the Keepsake.\n");
    EXPECT_TRUE(game_.world().item_positions.at("keepsake").is_player());

    reply("You are welcome.");
    const auto next = sessions.run_turn("keeper", "Thank you.");
    ASSERT_TRUE(next.has_value()) << next.error().message;
    std::vector<std::string> roles;
    for (const auto &message : body_of(last_request())["messages"]) {
        roles.push_back(message["role"].get<std::string>());
    }
    EXPECT_EQ(roles, (std::vector<std::string>{"system", "user", "assistant", "tool", "tool",
                                               "tool", "assistant", "tool", "tool", "user"}));
}

TEST_F(WordlessTurnTest, CommitsTheRoundsScryWouldHaveCommitted) {
    const auto history_after = [&](const std::string_view closing) -> json {
        ct::TempDir saves("wordless");
        CartridgeGame game(untrimmed_world(), saves.path());
        (void)game.dispatch_player("talk keeper");
        NpcSessionManager sessions(game, endpoint());
        reply("Welcome.");
        EXPECT_TRUE(sessions.run_turn("keeper", "hello").has_value());
        script(closing);
        EXPECT_TRUE(sessions.run_turn("keeper", "May I have it?").has_value());
        auto snapshot = sessions.snapshot_conversations();
        EXPECT_TRUE(snapshot.has_value());
        return std::move((*snapshot)["keeper"]["messages"]);
    };
    const auto wordless = history_after("");
    auto spoken = history_after("There you are.");
    ASSERT_EQ(spoken.size(), 8U);
    spoken.erase(spoken.size() - 1); // The closing message is the one difference.
    EXPECT_EQ(wordless, spoken);
}

TEST_F(WordlessTurnTest, AResponseCutOffInAToolCallLosesOnlyItself) {
    NpcSessionManager sessions(game_, endpoint());
    call({{.id = "c1", .name = "say", .arguments = R"({"text":"Welcome, Ishmael."})"}});
    // Ollama reports `length` beside the calls it managed to finish.
    auto cut_off = st::openai_tool_stream(
        {{.id = "c2", .name = "update_trust", .arguments = R"({"delta":9})"}});
    const std::string finish = "\"finish_reason\":\"tool_calls\"";
    ASSERT_NE(cut_off.find(finish), std::string::npos);
    cut_off.replace(cut_off.find(finish), finish.size(), "\"finish_reason\":\"length\"");
    server_.enqueue({.body_chunks = {cut_off}});

    const auto turn = sessions.run_turn("keeper", "I am Ishmael.");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Welcome, Ishmael.\"\n");
    EXPECT_EQ(game_.world().npcs.at("keeper").state.trust_toward_player, 0);

    reply("Ishmael.");
    ASSERT_TRUE(sessions.run_turn("keeper", "Who am I?").has_value());
    EXPECT_NE(last_request().body.find("I am Ishmael."), std::string::npos);
    EXPECT_EQ(last_request().body.find("update_trust\",\"arguments"), std::string::npos);
}

TEST_F(FailedTurnTest, TwoCutOffResponsesBeforeAnythingHappenedRollBack) {
    NpcSessionManager sessions(game_, endpoint());
    commit_good_turn(sessions);
    auto cut_off = st::openai_tool_stream(
        {{.id = "c2", .name = "update_trust", .arguments = R"({"delta":9})"}});
    const std::string finish = "\"finish_reason\":\"tool_calls\"";
    cut_off.replace(cut_off.find(finish), finish.size(), "\"finish_reason\":\"length\"");
    server_.enqueue({.body_chunks = {cut_off}});
    server_.enqueue({.body_chunks = {cut_off}});
    expect_rolled_back(sessions, "inconsistent");
}

TEST_F(WordlessTurnTest, AnthropicHistoryStaysSendable) {
    auto anthropic = endpoint();
    anthropic.anthropic_dialect = true;
    NpcSessionManager sessions(game_, anthropic);
    server_.enqueue({.body_chunks = {st::anthropic_tool_stream(
                         {{.id = "c1", .name = "say", .arguments = R"({"text":"Go."})"}})}});
    server_.enqueue({.body_chunks = {st::anthropic_text_stream("")}});
    ASSERT_TRUE(sessions.run_turn("keeper", "Shall I go?").has_value());

    server_.enqueue({.body_chunks = {st::anthropic_text_stream("Farewell.")}});
    ASSERT_TRUE(sessions.run_turn("keeper", "Goodbye.").has_value());
    // The Messages API takes alternating roles, so the result and the next
    // question share one user message.
    const auto messages = body_of(last_request())["messages"];
    ASSERT_EQ(messages.size(), 3U);
    EXPECT_EQ(messages[2]["content"][0]["type"], "tool_result");
    EXPECT_EQ(messages[2]["content"][1]["type"], "text");
}

TEST_F(WordlessTurnTest, SavesLoadsAndTrimsLikeAnyTurn) {
    WorldState world = ct::make_test_world();
    world.config.max_history_tokens = 250;
    ct::TempDir saves("wordlesstrim");
    CartridgeGame game(std::move(world), saves.path());
    ConsoleRuntime runtime(game, endpoint());
    (void)runtime.handle_line("talk keeper");
    for (int index = 0; index < 4; ++index) {
        const auto id = "call-" + std::to_string(index);
        const auto text = R"({"text":"answer-)" + std::to_string(index) + "\"}";
        call({{.id = id, .name = "say", .arguments = text}});
        reply("");
        const auto events = runtime.handle_line("question-" + std::to_string(index));
        EXPECT_NE(joined(events).find("answer-" + std::to_string(index)), std::string::npos);
        ASSERT_NE(joined(runtime.handle_line("save 1")).find("Saved"), std::string::npos);
        ASSERT_NE(joined(runtime.handle_line("load 1")).find("Loaded"), std::string::npos);

        const auto messages = body_of(last_request())["messages"];
        EXPECT_EQ(messages[1]["role"], "user");
    }
    const auto last = last_request().body;
    EXPECT_EQ(last.find("question-0"), std::string::npos);
    EXPECT_NE(last.find("question-2"), std::string::npos);
    EXPECT_NE(last.find("question-3"), std::string::npos);
}

// --- what the player hears -------------------------------------------------------

class SpeechTest : public NpcSessionTest {
  protected:
    void say(const std::string &text) {
        ids_.push_back("say-" + std::to_string(ids_.size()));
        bodies_.push_back(json{{"text", text}}.dump());
        call({{.id = ids_.back(), .name = "say", .arguments = bodies_.back()}});
    }

    std::vector<std::string> ids_;
    std::vector<std::string> bodies_;
};

TEST_F(SpeechTest, RepeatedLinesAreRefusedAndHeardOnce) {
    NpcSessionManager sessions(game_, endpoint());
    say("Who goes there?");
    say("Who goes there?");
    reply("");
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Who goes there?\"\n");
    EXPECT_EQ(tool_results(last_request()).back()["error"],
              "You already said that. End your turn.");

    // A new turn may open with the same words.
    say("Who goes there?");
    reply("");
    const auto next = sessions.run_turn("keeper", "hello?");
    ASSERT_TRUE(next.has_value()) << next.error().message;
    EXPECT_EQ(joined(*next), "Keeper: \"Who goes there?\"\n");
}

TEST_F(SpeechTest, LinesThatLookTheSameAreTheSame) {
    NpcSessionManager sessions(game_, endpoint());
    say("Hello there.");
    say(" Hello  there. ");
    say("\"Hello there.\"");
    reply("");
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Hello there.\"\n");
}

TEST_F(SpeechTest, AnNpcThatLeftIsNotHeardAnyMore) {
    NpcSessionManager sessions(game_, endpoint());
    call({{.id = "c1", .name = "move_self", .arguments = R"({"location_id":"garden"})"}});
    say("I am over here now.");
    reply("I am over here now.");
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper excuses themselves and leaves.\n");
}

TEST_F(SpeechTest, WordsBesideADepartureComeBeforeIt) {
    NpcSessionManager sessions(game_, endpoint());
    server_.enqueue(
        {.body_chunks = {st::openai_text_tool_stream(
             "Farewell.",
             {{.id = "c1", .name = "move_self", .arguments = R"({"location_id":"garden"})"}})}});
    reply("(gone)");
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Farewell.\"\nKeeper excuses themselves and leaves.\n");
}

TEST_F(SpeechTest, AnNpcThatWillNotStopTalkingIsHeardThreeTimes) {
    NpcSessionManager sessions(game_, endpoint());
    for (int line = 0; line < 9; ++line) {
        say("Line " + std::to_string(line));
    }
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Line 0\"\nKeeper: \"Line 1\"\nKeeper: \"Line 2\"\n");
    EXPECT_EQ(tool_results(last_request()).back()["error"], "You have said enough. End your turn.");
}

TEST_F(SpeechTest, SpeechIsOneLineWhateverTheModelSends) {
    NpcSessionManager sessions(game_, endpoint());
    say("  Wait.\n> quit\nGoodbye.\t\x01 ");
    reply("");
    auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Wait. > quit Goodbye.\"\n");

    reply("First.\n\nYou take the Crown.");
    turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"First. You take the Crown.\"\n");
}

TEST_F(SpeechTest, AnEmptySayIsRefusedAndTheAnswerIsHeard) {
    NpcSessionManager sessions(game_, endpoint());
    say(" \n ");
    reply("The gate is shut.");
    const auto turn = sessions.run_turn("keeper", "The gate?");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"The gate is shut.\"\n");
    EXPECT_EQ(tool_results(last_request()).back()["ok"], false);
}

TEST_F(SpeechTest, TextBesideToolCallsIsHeardWhenTheTurnEndsWithoutAny) {
    NpcSessionManager sessions(game_, endpoint());
    server_.enqueue({.body_chunks = {st::openai_text_tool_stream(
                         "You may\npass.",
                         {{.id = "c1", .name = "update_trust", .arguments = R"({"delta":1})"}})}});
    reply("");
    auto turn = sessions.run_turn("keeper", "May I?");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"You may pass.\"\n");
    EXPECT_EQ(game_.world().npcs.at("keeper").state.trust_toward_player, 1);

    // A closing line or a say replaces it.
    server_.enqueue(
        {.body_chunks = {st::openai_text_tool_stream(
             "Hm.", {{.id = "c2", .name = "update_trust", .arguments = R"({"delta":1})"}})}});
    reply("Very well.");
    turn = sessions.run_turn("keeper", "And now?");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Very well.\"\n");
    server_.enqueue({.body_chunks = {st::openai_text_tool_stream(
                         "I will say it.",
                         {{.id = "c3", .name = "say", .arguments = R"({"text":"Go on."})"}})}});
    reply("");
    turn = sessions.run_turn("keeper", "And now?");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper: \"Go on.\"\n");
}

template <std::size_t... Index>
std::string many_trust_calls(const std::vector<std::string> &ids, std::index_sequence<Index...>) {
    return st::openai_tool_stream({st::ToolUseBlock{
        .id = ids[Index], .name = "update_trust", .arguments = R"({"delta":1})"}...});
}

TEST_F(SpeechTest, OneResponseRunsAtMostThirtyTwoCalls) {
    NpcSessionManager sessions(game_, endpoint());
    std::vector<std::string> ids;
    for (int index = 0; index < 40; ++index) {
        ids.push_back("many-" + std::to_string(index));
    }
    server_.enqueue({.body_chunks = {many_trust_calls(ids, std::make_index_sequence<40>{})}});
    reply("Enough.");
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(game_.world().npcs.at("keeper").state.trust_toward_player, 32);
}

TEST_F(SpeechTest, RunningOutOfRoundsKeepsWhatTheToolsDid) {
    NpcSessionManager sessions(game_, endpoint());
    std::vector<std::string> ids;
    for (int round = 0; round < 9; ++round) {
        ids.push_back("loop-" + std::to_string(round));
        call({{.id = ids.back(), .name = "update_trust", .arguments = R"({"delta":5})"}});
    }
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_TRUE(turn.has_value()) << turn.error().message;
    EXPECT_EQ(joined(*turn), "Keeper gives no answer.\n");
    EXPECT_EQ(server_.requests().size(), 9U);
    // Eight rounds ran. The ninth asked for a tool that never did.
    EXPECT_EQ(game_.world().npcs.at("keeper").state.trust_toward_player, 40);

    reply("Where were we?");
    const auto next = sessions.run_turn("keeper", "hello?");
    ASSERT_TRUE(next.has_value()) << next.error().message;
    EXPECT_EQ(tool_results(last_request()).size(), 8U);
}

TEST_F(NpcSessionTest, AProvidersOwnReasonIsPartOfTheFailure) {
    NpcSessionManager sessions(game_, endpoint());
    server_.enqueue(
        {.status = 404, .body_chunks = {st::openai_error_body(404, "model_not_found")}});
    const auto turn = sessions.run_turn("keeper", "hello");
    ASSERT_FALSE(turn.has_value());
    EXPECT_NE(turn.error().message.find("(HTTP 404, openai:model_not_found)"), std::string::npos)
        << turn.error().message;
}

// --- history -----------------------------------------------------------------

TEST_F(NpcSessionTest, OnlyTheCurrentTurnCarriesItsContext) {
    NpcSessionManager sessions(game_, endpoint());
    for (const std::string text : {"first", "second", "third"}) {
        reply("Yes.");
        ASSERT_TRUE(sessions.run_turn("keeper", text).has_value());
    }
    const auto messages = body_of(last_request())["messages"];
    ASSERT_EQ(messages.size(), 6U);
    EXPECT_EQ(messages[1]["content"], "[Player]\n{\"player_said\":\"first\"}");
    EXPECT_EQ(messages[3]["content"], "[Player]\n{\"player_said\":\"second\"}");
    EXPECT_EQ(messages[5]["content"], build_npc_turn_message(game_.world(), "keeper", "third"));
    const auto snapshot = sessions.snapshot_conversations();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ((*snapshot)["keeper"]["messages"].dump().find("[Context]"), std::string::npos);
}

TEST_F(NpcSessionTest, TheLatestTurnSurvivesAnyBudget) {
    WorldState world = ct::make_test_world();
    world.config.max_history_tokens = 10;
    ct::TempDir saves("tinybudget");
    CartridgeGame game(std::move(world), saves.path());
    (void)game.dispatch_player("talk keeper");
    NpcSessionManager sessions(game, endpoint());
    for (const std::string text : {"first", "second", "third"}) {
        reply("answer to " + text);
        ASSERT_TRUE(sessions.run_turn("keeper", text).has_value());
    }
    const auto body = last_request().body;
    EXPECT_EQ(body.find("answer to first"), std::string::npos);
    EXPECT_NE(body.find("answer to second"), std::string::npos);
}

TEST_F(NpcSessionTest, HistoryTrimmingDropsTheOldestCompleteTurns) {
    // About two tool turns fit: 250 tokens is 1000 bytes of serialized history.
    WorldState world = ct::make_test_world();
    world.config.max_history_tokens = 250;
    ct::TempDir saves("trimsaves");
    CartridgeGame game(std::move(world), saves.path());
    (void)game.dispatch_player("talk keeper");
    NpcSessionManager sessions(game, endpoint());

    std::vector<std::string> ids;
    for (int index = 0; index < 4; ++index) {
        const auto marker = std::to_string(index);
        ids.push_back("call-" + marker);
        call({{.id = ids.back(), .name = "update_trust", .arguments = R"({"delta":1})"}});
        reply("answer-" + marker);
        const auto turn = sessions.run_turn("keeper", "question-" + marker);
        ASSERT_TRUE(turn.has_value()) << turn.error().message;

        // Whole turns go: what is left starts at a user message and every tool
        // result still follows the call it answers.
        const auto messages = body_of(last_request())["messages"];
        ASSERT_GE(messages.size(), 2U);
        EXPECT_EQ(messages[0]["role"], "system");
        EXPECT_EQ(messages[1]["role"], "user");
        for (std::size_t at = 1; at < messages.size(); ++at) {
            if (messages[at]["role"] == "tool") {
                ASSERT_TRUE(messages[at - 1].contains("tool_calls"));
                EXPECT_EQ(messages[at]["tool_call_id"], messages[at - 1]["tool_calls"][0]["id"]);
            }
        }
    }

    const auto last = last_request().body;
    EXPECT_EQ(last.find("question-0"), std::string::npos);
    EXPECT_EQ(last.find("answer-0"), std::string::npos);
    EXPECT_NE(last.find("question-2"), std::string::npos);
    EXPECT_NE(last.find("answer-2"), std::string::npos);
    EXPECT_NE(last.find("question-3"), std::string::npos);
    // Trimming touches history only; the world kept every accepted call.
    EXPECT_EQ(game.world().npcs.at("keeper").state.trust_toward_player, 4);

    const auto snapshot = sessions.snapshot_conversations();
    ASSERT_TRUE(snapshot.has_value()) << snapshot.error();
    EXPECT_EQ((*snapshot)["keeper"]["system_prompt"],
              build_npc_system_prompt(game.world(), "keeper"));
}

TEST_F(NpcSessionTest, ManyNpcsKeepTheirConversations) {
    // More than are kept live at once, so the first ones are set aside.
    WorldState world = untrimmed_world();
    for (int index = 0; index < 18; ++index) {
        auto npc = world.npcs.at("keeper");
        npc.identity.id = "guest_" + std::to_string(index);
        npc.identity.name = "Guest " + std::to_string(index);
        world.npcs[npc.identity.id] = npc;
    }
    ct::TempDir saves("manynpcs");
    CartridgeGame game(std::move(world), saves.path());
    NpcSessionManager sessions(game, endpoint());
    for (int index = 0; index < 18; ++index) {
        const auto id = "guest_" + std::to_string(index);
        reply("Yes.");
        ASSERT_TRUE(sessions.run_turn(id, "I am visitor " + std::to_string(index)).has_value());
    }
    const auto snapshot = sessions.snapshot_conversations();
    ASSERT_TRUE(snapshot.has_value()) << snapshot.error();
    ASSERT_EQ(snapshot->size(), 18U);
    for (int index = 0; index < 18; ++index) {
        const auto messages = (*snapshot)["guest_" + std::to_string(index)]["messages"].dump();
        EXPECT_NE(messages.find("I am visitor " + std::to_string(index) + "\\"), std::string::npos)
            << messages;
        EXPECT_EQ(messages.find("[Context]"), std::string::npos);
    }
    reply("Visitor 0.");
    ASSERT_TRUE(sessions.run_turn("guest_0", "Who am I?").has_value());
    EXPECT_NE(last_request().body.find("I am visitor 0"), std::string::npos);
}

TEST_F(NpcSessionTest, RememberKeepsItsImportance) {
    NpcSessionManager sessions(game_, endpoint());
    call({{.id = "c1", .name = "remember", .arguments = R"({"summary":"A debt.","importance":9})"},
          {.id = "c2", .name = "remember", .arguments = R"({"summary":"A greeting."})"}});
    reply("Noted.");
    ASSERT_TRUE(sessions.run_turn("keeper", "hello").has_value());
    const auto &memories = game_.world().npcs.at("keeper").state.memories;
    ASSERT_EQ(memories.size(), 2U);
    EXPECT_EQ(memories[0].importance, 9);
    EXPECT_EQ(memories[1].importance, 5);
}

TEST_F(NpcSessionTest, ConversationSurvivesSnapshotAndRestore) {
    const auto play_first_turn = [&](NpcSessionManager &sessions) {
        call({{.id = "c1", .name = "remember", .arguments = R"({"summary":"Ishmael came."})"},
              {.id = "c2", .name = "give_item", .arguments = R"({"item_id":"old_coin"})"}});
        reply("I will remember you, 你好.");
        return sessions.run_turn("keeper", "I am Ishmael.").has_value();
    };

    NpcSessionManager original(game_, endpoint());
    ASSERT_TRUE(play_first_turn(original));
    const auto snapshot = original.snapshot_conversations();
    ASSERT_TRUE(snapshot.has_value()) << snapshot.error();

    NpcSessionManager restored(game_, endpoint());
    const auto status = restored.restore_conversations(*snapshot);
    ASSERT_TRUE(status.has_value()) << status.error();
    EXPECT_EQ(restored.snapshot_conversations(), snapshot);

    // Both sessions put the same next request on the wire.
    reply("Ishmael.");
    ASSERT_TRUE(original.run_turn("keeper", "Who am I?").has_value());
    const auto from_original = last_request().body;
    reply("Ishmael.");
    ASSERT_TRUE(restored.run_turn("keeper", "Who am I?").has_value());
    EXPECT_EQ(last_request().body, from_original);
    EXPECT_NE(from_original.find("I am Ishmael."), std::string::npos);
    EXPECT_EQ(restored.snapshot_conversations(), original.snapshot_conversations());
}

TEST_F(NpcSessionTest, ConsoleSaveAndLoadCarryTheConversation) {
    ConsoleRuntime runtime(game_, endpoint());
    call({{.id = "c1", .name = "say", .arguments = R"({"text":"Noted, Queequeg."})"}});
    reply("(nods)");
    (void)runtime.handle_line("I am Queequeg.");
    ASSERT_NE(joined(runtime.handle_line("save 1")).find("Saved to slot 1"), std::string::npos);

    reply("A forgotten turn.");
    (void)runtime.handle_line("Forget this.");
    ASSERT_NE(joined(runtime.handle_line("load 1")).find("Loaded slot 1"), std::string::npos);

    reply("Queequeg.");
    EXPECT_NE(joined(runtime.handle_line("Who am I?")).find("Queequeg."), std::string::npos);
    EXPECT_NE(last_request().body.find("I am Queequeg."), std::string::npos);
    EXPECT_NE(last_request().body.find("Noted, Queequeg."), std::string::npos);
    EXPECT_EQ(last_request().body.find("Forget this."), std::string::npos);
}

// The fixture is the conversation document of a save file that Chronicle wrote
// when it was built on scry v0.1.0: every block type, both error flags, unicode,
// escapes, and numbers at the edges of their ranges. Do not regenerate it.
class LegacySaveTest : public NpcSessionTest {
  protected:
    [[nodiscard]] json legacy_conversations() const {
        std::ifstream stream(ct::repo_dir() / "tests" / "fixtures" /
                             "conversation_scry_v0_1_0.json");
        return {{"keeper", json::parse(stream)}};
    }
};

TEST_F(LegacySaveTest, ConversationSavedByScry010Loads) {
    const auto saved = legacy_conversations();
    NpcSessionManager sessions(game_, endpoint());
    const auto status = sessions.restore_conversations(saved);
    ASSERT_TRUE(status.has_value()) << status.error();
    // Every message survives, under today's prompt. v0.1.0 kept each turn's context;
    // only the player's words remain of it.
    auto expected = saved;
    expected["keeper"]["system_prompt"] = build_npc_system_prompt(game_.world(), "keeper");
    for (auto &message : expected["keeper"]["messages"]) {
        auto &block = message["content"][0];
        if (message["role"] == "user" && block["type"] == "text") {
            block["text"] = without_turn_context(block["text"]);
        }
    }
    ASSERT_NE(saved.dump().find("[Context]\\n"), std::string::npos);
    ASSERT_EQ(expected["keeper"]["messages"].dump().find("[Context]"), std::string::npos);
    EXPECT_EQ(sessions.snapshot_conversations(), expected);

    reply("I remember.");
    ASSERT_TRUE(sessions.run_turn("keeper", "Go on.").has_value());
    // The old turns go back to the model, and the new one is appended to them.
    const auto messages = body_of(last_request())["messages"];
    EXPECT_NE(messages[1]["content"].get<std::string>().find("你好"), std::string::npos);
    const auto after = sessions.snapshot_conversations();
    ASSERT_TRUE(after.has_value()) << after.error();
    const auto &before_messages = expected["keeper"]["messages"];
    const auto &after_messages = (*after)["keeper"]["messages"];
    ASSERT_EQ(after_messages.size(), before_messages.size() + 2);
    for (std::size_t index = 0; index < before_messages.size(); ++index) {
        EXPECT_EQ(after_messages[index], before_messages[index]) << index;
    }
}

TEST_F(LegacySaveTest, UnreadableConversationIsRefusedBeforeAnythingChanges) {
    NpcSessionManager sessions(game_, endpoint());
    reply("Live turn.");
    ASSERT_TRUE(sessions.run_turn("keeper", "hello").has_value());
    const auto live = sessions.snapshot_conversations();
    ASSERT_TRUE(live.has_value());

    const auto expect_refused = [&](const json &conversations, const std::string &reason) {
        const auto status = sessions.restore_conversations(conversations);
        ASSERT_FALSE(status.has_value());
        EXPECT_NE(status.error().find("conversation for 'keeper' is invalid"), std::string::npos);
        EXPECT_NE(status.error().find(reason), std::string::npos) << status.error();
        EXPECT_EQ(sessions.snapshot_conversations(), live);
    };

    auto future = legacy_conversations();
    future["keeper"]["version"] = 2;
    expect_refused(future, "version is not supported");

    auto incomplete = legacy_conversations();
    incomplete["keeper"]["messages"][0].erase("role");
    expect_refused(incomplete, "$.messages[0].role is a required member");

    reply("Still here.");
    EXPECT_TRUE(sessions.run_turn("keeper", "hello?").has_value());
}

} // namespace
} // namespace chronicle
