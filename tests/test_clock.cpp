#include "entities/clock.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

// FIXTURES_DIR is defined by CMake for test fixture path resolution
// static constexpr const char* kFixturesDir = FIXTURES_DIR;

namespace chronicle {

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(ClockTest, DefaultConstruction) {
    Clock c;
    EXPECT_EQ(c.day, 1);
    EXPECT_EQ(c.period, TimePeriod::Morning);
    EXPECT_EQ(c.turns_this_period, 0);
    EXPECT_EQ(c.total_turns, 0);
}

// ---------------------------------------------------------------------------
// advance_turn — within a period
// ---------------------------------------------------------------------------

TEST(ClockTest, AdvanceTurnWithinPeriod) {
    Clock c;
    for (int i = 0; i < 4; ++i)
        c.advance_turn(5);
    EXPECT_EQ(c.turns_this_period, 4);
    EXPECT_EQ(c.period, TimePeriod::Morning);
    EXPECT_EQ(c.total_turns, 4);
}

// ---------------------------------------------------------------------------
// advance_turn — period boundary
// ---------------------------------------------------------------------------

TEST(ClockTest, AdvanceTurnCrossesPeriodBoundary) {
    Clock c;
    for (int i = 0; i < 5; ++i)
        c.advance_turn(5);
    EXPECT_EQ(c.turns_this_period, 0);
    EXPECT_EQ(c.period, TimePeriod::Afternoon);
    EXPECT_EQ(c.total_turns, 5);
}

// ---------------------------------------------------------------------------
// Period roll-over Morning→Afternoon→Evening→Night→Morning (day++)
// ---------------------------------------------------------------------------

TEST(ClockTest, PeriodRolloverFullDay) {
    Clock c;
    // Advance through all 4 periods (5 turns each)
    for (int i = 0; i < 20; ++i)
        c.advance_turn(5);
    // After 4 full periods we should be back to Morning, Day 2
    EXPECT_EQ(c.period, TimePeriod::Morning);
    EXPECT_EQ(c.day, 2);
    EXPECT_EQ(c.total_turns, 20);
    EXPECT_EQ(c.turns_this_period, 0);
}

TEST(ClockTest, PeriodSequence) {
    Clock c;
    c.advance_turn(1); // turn 1: Morning→Afternoon
    EXPECT_EQ(c.period, TimePeriod::Afternoon);
    c.advance_turn(1); // turn 2: Afternoon→Evening
    EXPECT_EQ(c.period, TimePeriod::Evening);
    c.advance_turn(1); // turn 3: Evening→Night
    EXPECT_EQ(c.period, TimePeriod::Night);
    c.advance_turn(1); // turn 4: Night→Morning, day 2
    EXPECT_EQ(c.period, TimePeriod::Morning);
    EXPECT_EQ(c.day, 2);
}

// ---------------------------------------------------------------------------
// is_final_period
// ---------------------------------------------------------------------------

TEST(ClockTest, IsFinalPeriodFalseAtStart) {
    Clock c; // Morning Day 1 → elapsed = 0
    EXPECT_FALSE(c.is_final_period(1));
}

TEST(ClockTest, IsFinalPeriodTrueAfterOnePeriodAdvance) {
    Clock c;
    c.advance_turn(1); // now Afternoon, elapsed = 1
    EXPECT_TRUE(c.is_final_period(1));
}

TEST(ClockTest, IsFinalPeriodMultiDay) {
    Clock c;
    // Advance to Day 2, Morning (elapsed=4)
    for (int i = 0; i < 4; ++i)
        c.advance_turn(1);
    EXPECT_EQ(c.day, 2);
    EXPECT_EQ(c.period, TimePeriod::Morning);
    EXPECT_FALSE(c.is_final_period(5)); // elapsed=4, NOT >= 5
    EXPECT_TRUE(c.is_final_period(4));  // elapsed=4, >= 4
}

// ---------------------------------------------------------------------------
// Display / prompt strings
// ---------------------------------------------------------------------------

TEST(ClockTest, ToDisplayString) {
    Clock c;
    EXPECT_EQ(c.to_display_string(), "Morning, Day 1");
}

TEST(ClockTest, ToPromptString) {
    Clock c;
    EXPECT_EQ(c.to_prompt_string(), "Morning of Day 1");
}

TEST(ClockTest, ToDisplayStringAfternoonDay3) {
    Clock c;
    c.day = 3;
    c.period = TimePeriod::Afternoon;
    EXPECT_EQ(c.to_display_string(), "Afternoon, Day 3");
    EXPECT_EQ(c.to_prompt_string(), "Afternoon of Day 3");
}

// ---------------------------------------------------------------------------
// JSON roundtrip
// ---------------------------------------------------------------------------

TEST(ClockTest, JsonRoundtrip) {
    Clock original;
    original.day = 3;
    original.period = TimePeriod::Evening;
    original.turns_this_period = 2;
    original.total_turns = 42;

    nlohmann::json j;
    to_json(j, original);

    Clock restored;
    from_json(j, restored);

    EXPECT_EQ(restored.day, original.day);
    EXPECT_EQ(restored.period, original.period);
    EXPECT_EQ(restored.turns_this_period, original.turns_this_period);
    EXPECT_EQ(restored.total_turns, original.total_turns);
}

TEST(ClockTest, JsonPeriodSerializedAsString) {
    Clock c;
    c.period = TimePeriod::Night;
    nlohmann::json j;
    to_json(j, c);
    EXPECT_EQ(j["period"].get<std::string>(), "night");
}

// ---------------------------------------------------------------------------
// string_to_time_period — invalid input
// ---------------------------------------------------------------------------

TEST(ClockTest, StringToTimePeriodThrowsOnUnknown) {
    EXPECT_THROW(string_to_time_period("noon"), std::invalid_argument);
    EXPECT_THROW(string_to_time_period(""), std::invalid_argument);
    EXPECT_THROW(string_to_time_period("Morning"), std::invalid_argument); // case-sensitive
}

TEST(ClockTest, StringToTimePeriodAllValid) {
    EXPECT_EQ(string_to_time_period("morning"), TimePeriod::Morning);
    EXPECT_EQ(string_to_time_period("afternoon"), TimePeriod::Afternoon);
    EXPECT_EQ(string_to_time_period("evening"), TimePeriod::Evening);
    EXPECT_EQ(string_to_time_period("night"), TimePeriod::Night);
}

} // namespace chronicle
