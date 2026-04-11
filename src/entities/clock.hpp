#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

enum class TimePeriod { Morning, Afternoon, Evening, Night };

std::string time_period_to_string(TimePeriod p);
TimePeriod string_to_time_period(const std::string &s);

void to_json(nlohmann::json &j, const TimePeriod &p);
void from_json(const nlohmann::json &j, TimePeriod &p);

/// Tracks in-game time. Advances by turns; rolls through Morning/Afternoon/Evening/Night.
struct Clock {
    int day = 1;
    TimePeriod period = TimePeriod::Morning;
    int turns_this_period = 0;
    int total_turns = 0;

    /// Increment by one turn. Advances period when turns_this_period reaches turns_per_period.
    void advance_turn(int turns_per_period);

    /// Returns true once the total elapsed periods >= total_periods.
    bool is_final_period(int total_periods) const;

    /// "Morning, Day 1"
    std::string to_display_string() const;

    /// "Morning of Day 1"  (for LLM prompt context)
    std::string to_prompt_string() const;
};

void to_json(nlohmann::json &j, const Clock &c);
void from_json(const nlohmann::json &j, Clock &c);

} // namespace chronicle
