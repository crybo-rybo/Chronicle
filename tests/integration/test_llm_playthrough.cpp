// Live playthrough tests against a local Ollama server.
//
// These are opt-in (-DCHRONICLE_INTEGRATION_TESTS=ON) and skip cleanly when
// no server or model is available. Model preference follows the locally
// installed 8B-class models; override with CHRONICLE_MODEL. LLM output is
// nondeterministic, so assertions target mechanics invariants (a dialogue
// event happened, the gate applied a tool, the world stayed consistent)
// rather than exact text.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "../helpers.hpp"
#include "chronicle/cli.hpp"
#include "chronicle/llm/npc_sessions.hpp"
#include "chronicle/runtime.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;

constexpr const char *OLLAMA_URL = "http://127.0.0.1:11434";

std::string run_command(const std::string &command) {
    std::array<char, 4096> buffer{};
    std::string output;
    std::unique_ptr<FILE, int (*)(FILE *)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return output;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    return output;
}

// Pick a model: CHRONICLE_MODEL, else the first preferred model that is
// installed, else empty (skip).
std::string detect_model() {
    if (const char *forced = std::getenv("CHRONICLE_MODEL"); forced != nullptr && *forced != '\0') {
        return forced;
    }
    const std::string raw =
        run_command(std::string("curl -sf --max-time 5 ") + OLLAMA_URL + "/api/tags");
    if (raw.empty()) {
        return "";
    }
    const auto parsed = nlohmann::json::parse(raw, nullptr, false);
    if (parsed.is_discarded() || !parsed.contains("models")) {
        return "";
    }
    std::vector<std::string> installed;
    for (const auto &model : parsed["models"]) {
        if (model.contains("name")) {
            installed.push_back(model["name"].get<std::string>());
        }
    }
    for (const std::string preferred : {"qwen3:8b", "qwen3.5:9b", "lfm2.5:8b"}) {
        for (const auto &name : installed) {
            if (name == preferred || name.starts_with(preferred + ":")) {
                return name;
            }
        }
    }
    return installed.empty() ? "" : installed.front();
}

const std::string &cached_model() {
    static const std::string model = detect_model();
    return model;
}

#define SKIP_WITHOUT_MODEL()                                                                       \
    const std::string &model = cached_model();                                                     \
    if (model.empty()) {                                                                           \
        GTEST_SKIP() << "No Ollama server/model available at " << OLLAMA_URL;                      \
    }

EndpointConfig make_endpoint(const std::string &model) {
    EndpointConfig endpoint;
    endpoint.base_url = std::string(OLLAMA_URL) + "/v1";
    endpoint.model = model;
    endpoint.disable_reasoning = true; // qwen3-class models otherwise think at length
    endpoint.temperature = 0.3;        // keep playthroughs boring on purpose
    endpoint.max_tokens = 512;
    endpoint.timeout_ms = 180'000;
    return endpoint;
}

bool has_kind(const GameEvents &events, const EventKind kind) {
    for (const auto &event : events) {
        if (event.kind == kind) {
            return true;
        }
    }
    return false;
}

std::string joined(const GameEvents &events) {
    std::string all;
    for (const auto &event : events) {
        all += event.text + "\n";
    }
    return all;
}

TEST(LlmPlaythrough, ConversationProducesDialogue) {
    SKIP_WITHOUT_MODEL();
    ct::TempDir saves("it_dialogue");
    CartridgeGame game(ct::minimal_example(), saves.path());
    ConsoleRuntime runtime(game, make_endpoint(model));
    ASSERT_FALSE(runtime.using_stub());

    const auto greet = runtime.handle_line("talk warden");
    EXPECT_TRUE(has_kind(greet, EventKind::dialogue)) << joined(greet);

    const auto reply = runtime.handle_line("Hello! Who are you and what is this place?");
    EXPECT_TRUE(has_kind(reply, EventKind::dialogue)) << joined(reply);
    EXPECT_FALSE(has_kind(reply, EventKind::warning)) << joined(reply);
}

TEST(LlmPlaythrough, ExplicitToolInstructionRevealsKnowledge) {
    SKIP_WITHOUT_MODEL();
    ct::TempDir saves("it_reveal");
    CartridgeGame game(ct::minimal_example(), saves.path());
    ConsoleRuntime runtime(game, make_endpoint(model));

    (void)runtime.handle_line("talk warden");
    // Two attempts damp sampling flakiness; the instruction is deliberately
    // explicit because this tests the tool path, not model cleverness.
    for (int attempt = 0; attempt < 2; ++attempt) {
        (void)runtime.handle_line("Please use your reveal_knowledge tool now to share the "
                                  "fact with id fact_guest_arrived with me.");
        if (game.world().revealed_facts.contains("fact_guest_arrived")) {
            break;
        }
    }
    EXPECT_TRUE(game.world().revealed_facts.contains("fact_guest_arrived"));
}

TEST(LlmPlaythrough, GateRejectionDegradesGracefully) {
    SKIP_WITHOUT_MODEL();
    ct::TempDir saves("it_reject");
    CartridgeGame game(ct::minimal_example(), saves.path());
    ConsoleRuntime runtime(game, make_endpoint(model));

    (void)runtime.handle_line("talk warden");
    // The warden knows no fact with this id; a compliant model will try the
    // tool and the gate must reject it as a model-visible error without
    // corrupting the turn or the world.
    const auto events = runtime.handle_line(
        "Use your reveal_knowledge tool with fact id fact_hidden_treasure right now.");
    EXPECT_FALSE(game.world().revealed_facts.contains("fact_hidden_treasure"));
    EXPECT_EQ(game.world().revealed_facts.size(), 0u) << joined(events);
    EXPECT_NE(game.phase(), GamePhase::game_over);
}

TEST(LlmPlaythrough, ConversationMemoryPersistsAcrossTurns) {
    SKIP_WITHOUT_MODEL();
    ct::TempDir saves("it_memory");
    CartridgeGame game(ct::minimal_example(), saves.path());
    ConsoleRuntime runtime(game, make_endpoint(model));

    (void)runtime.handle_line("talk warden");
    (void)runtime.handle_line("Remember this: my name is Ishmael. Please greet me by name.");
    const auto reply =
        runtime.handle_line("What is my name? Answer with just the name I told you.");
    EXPECT_NE(joined(reply).find("Ishmael"), std::string::npos) << joined(reply);
}

TEST(LlmPlaythrough, ConversationSurvivesSaveAndLoad) {
    SKIP_WITHOUT_MODEL();
    ct::TempDir saves("it_saveload");
    CartridgeGame game(ct::minimal_example(), saves.path());
    ConsoleRuntime runtime(game, make_endpoint(model));

    (void)runtime.handle_line("talk warden");
    (void)runtime.handle_line("Remember this: my name is Queequeg.");
    (void)runtime.handle_line("save 1");
    // Wipe the live session state, then restore it from the save file.
    (void)runtime.handle_line("bye");
    (void)runtime.handle_line("load 1");
    ASSERT_EQ(game.phase(), GamePhase::in_conversation);
    const auto reply =
        runtime.handle_line("What is my name? Answer with just the name I told you.");
    EXPECT_NE(joined(reply).find("Queequeg"), std::string::npos) << joined(reply);
}

TEST(LlmPlaythrough, TinyWorldSmoke) {
    SKIP_WITHOUT_MODEL();
    ct::TempDir saves("it_tiny");
    CartridgeGame game(build_tiny_world(), saves.path());
    ConsoleRuntime runtime(game, make_endpoint(model));

    (void)runtime.handle_line("talk stranger");
    const auto reply = runtime.handle_line("Say exactly one short sentence of greeting.");
    EXPECT_TRUE(has_kind(reply, EventKind::dialogue)) << joined(reply);
}

} // namespace
} // namespace chronicle
