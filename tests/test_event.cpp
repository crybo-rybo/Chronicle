#include "entities/event.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

namespace chronicle {

// ---------------------------------------------------------------------------
// ConditionType string roundtrip — all 7 values
// ---------------------------------------------------------------------------

TEST(EventTest, ConditionTypeRoundTrip) {
    const ConditionType values[] = {
        ConditionType::ClockIs,    ConditionType::PlayerAt, ConditionType::FlagSet,
        ConditionType::NpcTrustGe, ConditionType::NpcAt,    ConditionType::ItemInPlayerInv,
        ConditionType::TurnGe,
    };
    for (auto t : values) {
        EXPECT_EQ(string_to_condition_type(condition_type_to_string(t)), t);
    }
}

// ---------------------------------------------------------------------------
// Unknown string throws std::invalid_argument
// ---------------------------------------------------------------------------

TEST(EventTest, ConditionTypeStringUnknownThrows) {
    EXPECT_THROW(string_to_condition_type("garbage"), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Condition JSON roundtrip
// ---------------------------------------------------------------------------

TEST(EventTest, ConditionJsonRoundTrip) {
    Condition original;
    original.type = ConditionType::NpcAt;
    original.args = {"innkeeper", "tavern"};

    nlohmann::json j;
    to_json(j, original);

    Condition restored;
    from_json(j, restored);

    EXPECT_EQ(restored.type, original.type);
    EXPECT_EQ(restored.args, original.args);
}

// ---------------------------------------------------------------------------
// EventAction JSON roundtrip
// ---------------------------------------------------------------------------

TEST(EventTest, EventActionJsonRoundTrip) {
    EventAction original;
    original.type = "narrate";
    original.params = {{"text", "Something happened."}};

    nlohmann::json j;
    to_json(j, original);

    EventAction restored;
    from_json(j, restored);

    EXPECT_EQ(restored.type, original.type);
    EXPECT_EQ(restored.params, original.params);
}

// ---------------------------------------------------------------------------
// EventTrigger JSON roundtrip — full trigger with 2 conditions and 1 action
// ---------------------------------------------------------------------------

TEST(EventTest, EventTriggerJsonRoundTrip) {
    EventTrigger original;
    original.id = "market_commotion";

    Condition c1;
    c1.type = ConditionType::ClockIs;
    c1.args = {"afternoon"};

    Condition c2;
    c2.type = ConditionType::PlayerAt;
    c2.args = {"market_square"};

    original.conditions = {c1, c2};

    EventAction a1;
    a1.type = "narrate";
    a1.params = {{"text", "A commotion breaks out."}};

    original.actions = {a1};
    original.once = true;
    original.fired = true;

    nlohmann::json j;
    to_json(j, original);

    EventTrigger restored;
    from_json(j, restored);

    EXPECT_EQ(restored.id, original.id);
    ASSERT_EQ(restored.conditions.size(), 2u);
    EXPECT_EQ(restored.conditions[0].type, ConditionType::ClockIs);
    EXPECT_EQ(restored.conditions[0].args, std::vector<std::string>{"afternoon"});
    EXPECT_EQ(restored.conditions[1].type, ConditionType::PlayerAt);
    EXPECT_EQ(restored.conditions[1].args, std::vector<std::string>{"market_square"});
    ASSERT_EQ(restored.actions.size(), 1u);
    EXPECT_EQ(restored.actions[0].type, "narrate");
    EXPECT_EQ(restored.actions[0].params.at("text"), "A commotion breaks out.");
    EXPECT_EQ(restored.once, original.once);
    EXPECT_EQ(restored.fired, original.fired);
}

// ---------------------------------------------------------------------------
// Default-constructed EventTrigger: fired=false, once=true
// ---------------------------------------------------------------------------

TEST(EventTest, EventTriggerDefaultFired) {
    EventTrigger t;
    EXPECT_FALSE(t.fired);
    EXPECT_TRUE(t.once);
}

} // namespace chronicle
