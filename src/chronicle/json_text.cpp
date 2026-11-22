#include "chronicle/json_text.hpp"

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace chronicle {

using nlohmann::json;

json parse_untrusted_json(const std::string_view text, const std::size_t max_depth) {
    // The keys seen so far in each open object; arrays hold an empty set.
    std::vector<std::set<std::string>> open;
    const auto check = [&](int, const json::parse_event_t event, json &parsed) {
        switch (event) {
        case json::parse_event_t::object_start:
        case json::parse_event_t::array_start:
            if (open.size() >= max_depth) {
                throw std::invalid_argument("nested more than " + std::to_string(max_depth) +
                                            " levels deep");
            }
            open.emplace_back();
            break;
        case json::parse_event_t::object_end:
        case json::parse_event_t::array_end:
            open.pop_back();
            break;
        case json::parse_event_t::key:
            if (!open.back().insert(parsed.get_ref<const std::string &>()).second) {
                throw std::invalid_argument("repeats the key " + parsed.dump());
            }
            break;
        case json::parse_event_t::value:
            break;
        }
        return true;
    };
    return json::parse(text, check);
}

} // namespace chronicle
