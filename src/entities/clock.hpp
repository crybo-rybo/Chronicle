/**
 * @file clock.hpp
 * @brief In-game time tracking for Chronicle.
 *
 * @details The Clock drives temporal progression in the game world.  Time is
 * divided into discrete @ref TimePeriod values (Morning, Afternoon, Evening,
 * Night) within numbered days.  Each significant player action advances the
 * clock by one turn; when enough turns accumulate within a period the period
 * rolls forward, creating natural time pressure.
 */

#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

/// @brief The four time-of-day periods that make up a single game day.
///
/// Periods advance in order: Morning → Afternoon → Evening → Night → Morning
/// (next day).  The integer representation is relied upon by
/// @ref Clock::is_final_period; do not reorder these enumerators.
enum class TimePeriod { Morning, Afternoon, Evening, Night };

/// @brief Convert a @ref TimePeriod to its lowercase string representation.
/// @param p The time period to convert.
/// @return One of @c "morning", @c "afternoon", @c "evening", @c "night".
std::string time_period_to_string(TimePeriod p);

/// @brief Parse a lowercase string into a @ref TimePeriod.
/// @param s The string to parse.  Case-sensitive.
/// @return The matching @ref TimePeriod value.
/// @throws std::invalid_argument if @p s does not match any known period.
TimePeriod string_to_time_period(const std::string &s);

/// @cond INTERNAL
void to_json(nlohmann::json &j, const TimePeriod &p);
void from_json(const nlohmann::json &j, TimePeriod &p);
/// @endcond

/// @brief Tracks in-game time as a day number plus a named time period.
///
/// @details The clock advances turn-by-turn.  When @c turns_this_period
/// reaches the configured @p turns_per_period threshold, the period rolls
/// forward and @c turns_this_period resets to zero.  Rolling past @c Night
/// increments @c day and wraps back to @c Morning.
///
/// All fields are serialisable so the full clock state is preserved across
/// save/load cycles.
struct Clock {
    int day = 1;                             ///< Current game day, starting at 1.
    TimePeriod period = TimePeriod::Morning; ///< Current period within the day.
    int turns_this_period = 0;               ///< Turns elapsed since last period transition.
    int total_turns = 0;                     ///< Total turns elapsed across all days and periods.

    /// @brief Advance the clock by one turn.
    ///
    /// @details Increments @c turns_this_period and @c total_turns.  If
    /// @c turns_this_period reaches @p turns_per_period the period rolls
    /// forward; rolling past @c Night increments the day counter.
    ///
    /// @param turns_per_period Number of turns that constitute one period.
    /// @pre @p turns_per_period > 0.
    void advance_turn(int turns_per_period);

    /// @brief Test whether the resolution threshold has been reached.
    ///
    /// @details Converts the current day and period into a linear period
    /// count (e.g., Day 2 Afternoon == period index 5) and compares against
    /// @p total_periods.
    ///
    /// @param total_periods Total number of periods in the game before resolution fires.
    /// @return @c true once the elapsed period count is greater than or equal to
    ///         @p total_periods.
    bool is_final_period(int total_periods) const;

    /// @brief Short human-readable string for UI display.
    /// @return A string of the form @c "Morning, Day 1".
    std::string to_display_string() const;

    /// @brief Verbose string formatted for inclusion in LLM system prompts.
    /// @return A string of the form @c "Morning of Day 1".
    std::string to_prompt_string() const;
};

/// @cond INTERNAL
void to_json(nlohmann::json &j, const Clock &c);
void from_json(const nlohmann::json &j, Clock &c);
/// @endcond

} // namespace chronicle
