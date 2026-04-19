#include "entities/fact.hpp"

namespace chronicle {

void to_json(nlohmann::json &j, const Fact &f) {
    j = nlohmann::json{
        {"text", f.text},
        {"category", f.category},
        {"revealed_by_default", f.revealed_by_default},
    };
}

void from_json(const nlohmann::json &j, Fact &f) {
    j.at("text").get_to(f.text);
    j.at("category").get_to(f.category);
    j.at("revealed_by_default").get_to(f.revealed_by_default);
}

} // namespace chronicle
