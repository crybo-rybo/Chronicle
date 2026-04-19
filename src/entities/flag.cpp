#include "entities/flag.hpp"

namespace chronicle {

void to_json(nlohmann::json &j, const Flag &f) {
    j = nlohmann::json{
        {"default", f.default_value},
        {"description", f.description},
    };
}

void from_json(const nlohmann::json &j, Flag &f) {
    j.at("default").get_to(f.default_value);
    j.at("description").get_to(f.description);
}

} // namespace chronicle
