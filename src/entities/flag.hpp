#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

struct Flag {
    std::string id;
    bool default_value = false;
    std::string description;
};

void to_json(nlohmann::json &j, const Flag &f);
void from_json(const nlohmann::json &j, Flag &f);

} // namespace chronicle
