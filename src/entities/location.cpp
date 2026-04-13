/**
 * @file location.cpp
 * @brief Implementation of @ref Location methods and JSON serialisation.
 */

#include "entities/location.hpp"
#include "entities/world.hpp" // needed for World complete type

namespace chronicle {

std::string Location::describe(const World & /*world*/) const {
    if (!current_description.empty()) {
        return current_description;
    }
    return base_description;
}

void to_json(nlohmann::json &j, const Location &loc) {
    j = nlohmann::json{
        {"id", loc.id},
        {"name", loc.name},
        {"base_description", loc.base_description},
        {"current_description", loc.current_description},
        {"exits", loc.exits},
        {"items", loc.items},
        {"npcs", loc.npcs},
        {"locked_exits", loc.locked_exits},
    };
}

void from_json(const nlohmann::json &j, Location &loc) {
    loc.id = j.at("id").get<std::string>();
    loc.name = j.at("name").get<std::string>();
    loc.base_description = j.value("base_description", std::string{});
    loc.current_description = j.value("current_description", std::string{});
    loc.exits = j.value("exits", std::map<std::string, std::string>{});
    loc.items = j.value("items", std::vector<std::string>{});
    loc.npcs = j.value("npcs", std::vector<std::string>{});
    loc.locked_exits = j.value("locked_exits", std::vector<std::string>{});
}

} // namespace chronicle
