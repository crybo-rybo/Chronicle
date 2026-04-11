#include "entities/clock.hpp"
#include <cctype>
#include <stdexcept>

namespace chronicle {

std::string time_period_to_string(TimePeriod p) {
    switch (p) {
    case TimePeriod::Morning:
        return "morning";
    case TimePeriod::Afternoon:
        return "afternoon";
    case TimePeriod::Evening:
        return "evening";
    case TimePeriod::Night:
        return "night";
    }
    // Unreachable, but suppress compiler warning
    return "morning";
}

TimePeriod string_to_time_period(const std::string &s) {
    if (s == "morning")
        return TimePeriod::Morning;
    if (s == "afternoon")
        return TimePeriod::Afternoon;
    if (s == "evening")
        return TimePeriod::Evening;
    if (s == "night")
        return TimePeriod::Night;
    throw std::invalid_argument("Unknown time period: " + s);
}

void to_json(nlohmann::json &j, const TimePeriod &p) { j = time_period_to_string(p); }

void from_json(const nlohmann::json &j, TimePeriod &p) {
    p = string_to_time_period(j.get<std::string>());
}

void Clock::advance_turn(int turns_per_period) {
    ++turns_this_period;
    ++total_turns;
    if (turns_this_period >= turns_per_period) {
        turns_this_period = 0;
        switch (period) {
        case TimePeriod::Morning:
            period = TimePeriod::Afternoon;
            break;
        case TimePeriod::Afternoon:
            period = TimePeriod::Evening;
            break;
        case TimePeriod::Evening:
            period = TimePeriod::Night;
            break;
        case TimePeriod::Night:
            period = TimePeriod::Morning;
            ++day;
            break;
        }
    }
}

bool Clock::is_final_period(int total_periods) const {
    // NOTE: Relies on TimePeriod members having sequential values Morning=0..Night=3 per declaration order.
    int elapsed = (day - 1) * 4 + static_cast<int>(period);
    return elapsed >= total_periods;
}

std::string Clock::to_display_string() const {
    // Capitalise first letter of period
    std::string p = time_period_to_string(period);
    p[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(p[0]))); // 'a'->'A' etc.
    return p + ", Day " + std::to_string(day);
}

std::string Clock::to_prompt_string() const {
    std::string p = time_period_to_string(period);
    p[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(p[0])));
    return p + " of Day " + std::to_string(day);
}

void to_json(nlohmann::json &j, const Clock &c) {
    j = nlohmann::json{{"day", c.day},
                       {"period", c.period},
                       {"turns_this_period", c.turns_this_period},
                       {"total_turns", c.total_turns}};
}

void from_json(const nlohmann::json &j, Clock &c) {
    j.at("day").get_to(c.day);
    j.at("period").get_to(c.period);
    j.at("turns_this_period").get_to(c.turns_this_period);
    j.at("total_turns").get_to(c.total_turns);
}

} // namespace chronicle
