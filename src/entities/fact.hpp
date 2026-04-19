#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace chronicle {

struct Fact {
    std::string id;
    std::string text;
    std::string category;
    bool revealed_by_default = false;
};

void to_json(nlohmann::json &j, const Fact &f);
void from_json(const nlohmann::json &j, Fact &f);

} // namespace chronicle
