#include <chrono>
#include <functional>
#include <gtest/gtest.h>

#include "chronicle/cartridge/validator.hpp"
#include "helpers.hpp"

namespace chronicle {
namespace {

namespace ct = chronicle::testing;

bool has_issue_containing(const std::vector<ValidationIssue> &issues, const std::string &needle,
                          const IssueLevel level = IssueLevel::error) {
    for (const auto &issue : issues) {
        if (issue.level == level && issue.message.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

TEST(Validator, BundledExamplesAreValid) {
    EXPECT_FALSE(has_errors(validate_package(ct::minimal_example())));
    EXPECT_FALSE(has_errors(validate_package(ct::broken_wheel_example())));
}

TEST(Validator, TestWorldIsValid) {
    const auto issues = validate_world(ct::make_test_world());
    EXPECT_FALSE(has_errors(issues));
}

TEST(Validator, SchemaVersionMismatch) {
    WorldState world = ct::make_test_world();
    world.manifest.chronicle_schema_version = 99;
    EXPECT_TRUE(has_issue_containing(validate_world(world), "chronicle_schema_version"));
}

TEST(Validator, EmptyScenarioId) {
    WorldState world = ct::make_test_world();
    world.manifest.id = "  ";
    EXPECT_TRUE(has_issue_containing(validate_world(world), "scenario id"));
}

TEST(Validator, ScenarioIdMustBeASafeDirectoryComponent) {
    for (const std::string id :
         {"../escape", "/absolute", "has/slash", "Uppercase", ".hidden", "has space", ""}) {
        WorldState world = ct::make_test_world();
        world.manifest.id = id;
        EXPECT_TRUE(has_issue_containing(validate_world(world), "scenario id")) << id;
    }
    EXPECT_TRUE(is_safe_cartridge_id("case-42_alpha"));
}

TEST(Validator, ConfigResourceBoundsAreEnforced) {
    WorldState world = ct::make_test_world();
    world.config.max_response_tokens = 1'000'000;
    world.config.inference_timeout_ms = 1;
    world.config.total_periods = 0;
    const auto issues = validate_world(world);
    EXPECT_TRUE(has_issue_containing(issues, "max_response_tokens"));
    EXPECT_TRUE(has_issue_containing(issues, "inference_timeout_ms"));
    EXPECT_TRUE(has_issue_containing(issues, "clock limits"));
}

TEST(Validator, AliasesThatDoNothingAreWarnedAbout) {
    const auto warned = [](const std::string &alias, const std::string &expansion,
                           const std::string &needle) {
        WorldState world = ct::make_test_world();
        world.config.verb_aliases[alias] = expansion;
        const auto issues = validate_world(world);
        EXPECT_FALSE(has_errors(issues)) << alias; // It plays all the same.
        return has_issue_containing(issues, needle, IssueLevel::warning);
    };
    EXPECT_FALSE(warned("grab", "take", "verb alias"));
    EXPECT_FALSE(warned("north", "Go north", "verb alias"));
    for (const std::string word : {"quit", "q", "save", "look", "bye", "leave"}) {
        EXPECT_TRUE(warned(word, "take", "redefines a built-in")) << word;
    }
    for (const std::string word : {"Grab", "pick up", ""}) {
        EXPECT_TRUE(warned(word, "take", "one lowercase word")) << word;
    }
    EXPECT_TRUE(warned("jump", "leap", "does not stand for"));
    EXPECT_TRUE(warned("jump", "", "does not stand for"));
}

TEST(Validator, UnknownStartLocation) {
    WorldState world = ct::make_test_world();
    world.player.current_location = "nowhere";
    EXPECT_TRUE(has_issue_containing(validate_world(world), "start_location"));
}

TEST(Validator, ExitToUnknownLocation) {
    WorldState world = ct::make_test_world();
    world.locations.at("hall").exits["west"] = "void";
    EXPECT_TRUE(has_issue_containing(validate_world(world), "exit west -> unknown void"));
}

TEST(Validator, BlankNamesAreWarnedAbout) {
    WorldState world = ct::make_test_world();
    world.locations.at("hall").name = " ";
    world.items.at("old_coin").name = "";
    world.npcs.at("keeper").identity.name = "\t";
    world.facts.at("fact_gate").text = "\n";
    const auto issues = validate_world(world);
    EXPECT_FALSE(has_errors(issues));
    for (const std::string expected :
         {"location hall: name is blank", "item old_coin: name is blank",
          "npc keeper: name is blank", "fact fact_gate: text is blank"}) {
        EXPECT_TRUE(has_issue_containing(issues, expected, IssueLevel::warning)) << expected;
    }
}

TEST(Validator, AnIssueIsOneLineWhateverItNames) {
    WorldState world = ct::make_test_world();
    world.locations.at("hall").exits["up"] = "void\nOK\r\nready:   true";
    const auto issues = validate_world(world);
    ASSERT_FALSE(issues.empty());
    EXPECT_EQ(issues.front().to_string(),
              "[error] location hall: exit up -> unknown void OK  ready:   true");
}

TEST(Validator, ExitsThatCannotBeNamedAreWarnedAbout) {
    WorldState world = ct::make_test_world();
    world.locations.at("hall").exits[" "] = "garden";
    world.locations.at("hall").exits["North"] = "garden";
    const auto issues = validate_world(world);
    EXPECT_FALSE(has_errors(issues));
    EXPECT_TRUE(has_issue_containing(issues, "blank direction", IssueLevel::warning));
    EXPECT_TRUE(has_issue_containing(issues, "differ only in case", IssueLevel::warning));
}

// Validation also guards every save and every rollback, so it must stay fast on
// anything a package may hold.
TEST(Validator, HugeListsAreValidatedInTime) {
    WorldState world = ct::make_test_world();
    for (int index = 0; index < 60'000; ++index) {
        world.locations.at("hall").exits["way " + std::to_string(index)] = "garden";
    }
    for (int index = 0; index < 500; ++index) {
        ItemData item;
        item.name = "Key";
        item.unlock_target = std::string(64, 'k') + std::to_string(index);
        world.items["key_" + std::to_string(index)] = item;
    }
    auto &identity = world.npcs.at("keeper").identity;
    for (int index = 0; index < 500; ++index) {
        const auto id = "fact_" + std::to_string(index);
        world.facts[id].text = "A fact.";
        identity.knowledge.push_back(id);
    }
    identity.tool_policy.allowed_facts.assign(60'000, "fact_499");
    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(has_errors(validate_world(world))); // The facts repeat.
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(5));
}

TEST(Validator, ThingsThatCanNeverHappenAreWarnedAbout) {
    const auto warns = [](const WorldState &world, const std::string &needle) {
        const auto issues = validate_world(world);
        EXPECT_FALSE(has_errors(issues));
        return has_issue_containing(issues, needle, IssueLevel::warning);
    };
    WorldState world = ct::make_test_world();
    EXPECT_TRUE(validate_world(world).empty());
    world.items.at("gate_key").unlock_target = "gardn";
    EXPECT_TRUE(warns(world, "unlock_target gardn"));
    world.items.at("gate_key").unlock_target = "North";
    EXPECT_FALSE(warns(world, "unlock_target"));

    world.events["finale"] = EventTriggerData{
        .conditions = {},
        .actions = {{.type = "end_game", .params = nlohmann::json::object()},
                    {.type = "narrate", .params = {{"text", "Too late."}}}},
    };
    EXPECT_TRUE(warns(world, "nothing happens after end_game"));
    world.events["finale"] = EventTriggerData{
        .conditions = {},
        .actions = {{.type = "spawn_item",
                     .params = {{"item_id", "hidden_note"}, {"location_id", "garden"}}}},
    };
    EXPECT_TRUE(warns(world, "hidden_note is hidden"));
}

TEST(Validator, UnplacedItemIsWarning) {
    WorldState world = ct::make_test_world();
    ItemData floating;
    floating.name = "Floating";
    floating.description = "Placed nowhere.";
    world.items["floating"] = floating;
    const auto issues = validate_world(world);
    EXPECT_FALSE(has_errors(issues));
    EXPECT_TRUE(has_issue_containing(issues, "not placed anywhere", IssueLevel::warning));
}

TEST(Validator, NpcIssues) {
    WorldState world = ct::make_test_world();
    auto &npc = world.npcs.at("keeper");
    npc.state.current_location = "void";
    npc.state.mood = "confused";
    npc.identity.knowledge.push_back("fact_missing");
    world.item_positions["item_missing"] = ItemPosition{.holder = ItemHolder::npc, .id = "keeper"};
    npc.identity.tool_policy.allowed_tools.push_back("teleport");
    npc.identity.tool_policy.allowed_items.push_back("item_missing");
    npc.identity.tool_policy.allowed_facts.push_back("fact_missing");
    npc.identity.tool_policy.allowed_flags.push_back("flag_missing");
    npc.identity.tool_policy.allowed_locations.push_back("void");
    const auto issues = validate_world(world);
    EXPECT_TRUE(has_issue_containing(issues, "unknown location void"));
    EXPECT_TRUE(has_issue_containing(issues, "invalid mood confused"));
    EXPECT_TRUE(has_issue_containing(issues, "unknown knowledge fact fact_missing"));
    EXPECT_TRUE(has_issue_containing(issues, "item position references unknown item item_missing"));
    EXPECT_TRUE(has_issue_containing(issues, "unknown tool teleport"));
    EXPECT_TRUE(has_issue_containing(issues, "allowed_items unknown item_missing"));
    EXPECT_TRUE(has_issue_containing(issues, "allowed_facts unknown fact_missing"));
    EXPECT_TRUE(has_issue_containing(issues, "allowed_flags unknown flag_missing"));
    EXPECT_TRUE(has_issue_containing(issues, "allowed_locations unknown void"));
}

TEST(Validator, ToolsWithNothingInScopeAreWarnedAbout) {
    WorldState world = ct::make_test_world();
    EXPECT_FALSE(
        has_issue_containing(validate_world(world), "every call is refused", IssueLevel::warning));
    auto &identity = world.npcs.at("keeper").identity;
    identity.tool_policy.allowed_items.clear();
    identity.tool_policy.allowed_facts.clear();
    identity.tool_policy.allowed_flags.clear();
    identity.tool_policy.allowed_locations.clear();
    auto issues = validate_world(world);
    EXPECT_FALSE(has_errors(issues));
    for (const std::string tool :
         {"give_item is allowed but allowed_items", "take_item", "inspect_item",
          "reveal_knowledge is allowed but allowed_facts", "set_flag is allowed but allowed_flags",
          "move_self is allowed but allowed_locations"}) {
        EXPECT_TRUE(has_issue_containing(issues, tool, IssueLevel::warning)) << tool;
    }

    // A list nobody uses is not worth a warning.
    identity.tool_policy.allowed_tools = {"say"};
    EXPECT_FALSE(
        has_issue_containing(validate_world(world), "every call is refused", IssueLevel::warning));

    identity.tool_policy.allowed_facts = {"fact_gate"};
    identity.knowledge.clear();
    EXPECT_TRUE(has_issue_containing(validate_world(world), "which is not in knowledge",
                                     IssueLevel::warning));
}

TEST(Validator, MutableRuntimeStateIsBoundedAndReferentiallyValid) {
    WorldState world = ct::make_test_world();
    world.clock.turns_elapsed = -1;
    world.clock.turns_per_period = 99;
    world.flags["injected"] = true;
    world.revealed_facts.insert("fact_missing");
    auto &npc = world.npcs.at("keeper");
    npc.state.trust_toward_player = 101;
    npc.state.memories.push_back({.timestamp = "morning",
                                  .type = "observation",
                                  .summary = " ",
                                  .importance = 11,
                                  .related_npc = "ghost",
                                  .related_item = "missing"});

    const auto issues = validate_world(world);
    EXPECT_TRUE(has_issue_containing(issues, "runtime clock"));
    EXPECT_TRUE(has_issue_containing(issues, "runtime flag set"));
    EXPECT_TRUE(has_issue_containing(issues, "revealed facts"));
    EXPECT_TRUE(has_issue_containing(issues, "trust values"));
    EXPECT_TRUE(has_issue_containing(issues, "memory summary"));
    EXPECT_TRUE(has_issue_containing(issues, "memory importance"));
    EXPECT_TRUE(has_issue_containing(issues, "memory references unknown NPC"));
    EXPECT_TRUE(has_issue_containing(issues, "memory references unknown item"));
}

TEST(Validator, AuthoredIdentityAndLockedExitIntegrityAreEnforced) {
    WorldState world = ct::make_test_world();
    world.npcs.at("keeper").identity.id = "impostor";
    world.npcs.at("keeper").identity.knowledge.push_back("fact_gate");
    world.locations.at("hall").locked_exits.push_back(
        LockedExitEntry{.direction = "north", .unlocked = false});

    const auto issues = validate_world(world);
    EXPECT_TRUE(has_issue_containing(issues, "identity id"));
    EXPECT_TRUE(has_issue_containing(issues, "duplicate 'fact_gate'"));
    EXPECT_TRUE(has_issue_containing(issues, "duplicate locked exit north"));
}

TEST(Validator, EventConditionIssues) {
    WorldState world = ct::make_test_world();
    world.events["bad"] = EventTriggerData{
        .conditions = {{.type = "player_at", .args = {"void"}},
                       {.type = "turn_ge", .args = {"soon"}},
                       {.type = "mystery", .args = {}}},
        .actions = {},
    };
    const auto issues = validate_world(world);
    EXPECT_TRUE(has_issue_containing(issues, "condition player_at: bad location"));
    EXPECT_TRUE(has_issue_containing(issues, "turn count must be a non-negative int"));
    EXPECT_TRUE(has_issue_containing(issues, "unknown condition type"));
}

TEST(Validator, NumericEventThresholdsStayInsideRuntimeDomains) {
    WorldState world = ct::make_test_world();
    world.events["bad_thresholds"] = EventTriggerData{
        .conditions = {{.type = "npc_trust_ge", .args = {"keeper", "101"}},
                       {.type = "turn_ge", .args = {"-1"}}},
        .actions = {},
    };
    const auto issues = validate_world(world);
    EXPECT_TRUE(has_issue_containing(issues, "bad npc/threshold"));
    EXPECT_TRUE(has_issue_containing(issues, "turn count must be a non-negative int"));
}

TEST(Validator, EventActionIssues) {
    WorldState world = ct::make_test_world();
    world.events["bad"] = EventTriggerData{
        .conditions = {},
        .actions = {{.type = "move_npc", .params = {{"npc_id", "ghost"}, {"location_id", "hall"}}},
                    {.type = "set_flag", .params = {{"flag_id", "flag_missing"}}},
                    {.type = "explode", .params = {}}},
    };
    const auto issues = validate_world(world);
    EXPECT_TRUE(has_issue_containing(issues, "action move_npc: bad npc/location"));
    EXPECT_TRUE(has_issue_containing(issues, "unknown flag flag_missing"));
    EXPECT_TRUE(has_issue_containing(issues, "unknown action type"));
}

TEST(Validator, EventActionParametersAreExactAndTyped) {
    WorldState world = ct::make_test_world();
    world.events["bad_params"] = EventTriggerData{
        .conditions = {},
        .actions = {{.type = "set_flag",
                     .params = {{"flag_id", "gate_seen"}, {"value", "true"}, {"extra", 1}}},
                    {.type = "narrate", .params = {{"text", 42}}}},
    };
    const auto issues = validate_world(world);
    EXPECT_TRUE(has_issue_containing(issues, "action set_flag"));
    EXPECT_TRUE(has_issue_containing(issues, "action narrate"));
}

TEST(Validator, LoadFailureBecomesSingleError) {
    const auto issues = validate_package("/nonexistent/nowhere");
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues.front().level, IssueLevel::error);
}

} // namespace
} // namespace chronicle
