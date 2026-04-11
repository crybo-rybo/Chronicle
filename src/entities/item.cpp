#include "entities/item.hpp"

namespace chronicle {

void to_json(nlohmann::json& j, const Item& item) {
    j = nlohmann::json{
        {"id", item.id},
        {"name", item.name},
        {"description", item.description},
        {"takeable", item.takeable},
        {"key_item", item.key_item},
        {"hidden", item.hidden},
        {"unlock_target", item.unlock_target},
        {"properties", item.properties},
    };
}

void from_json(const nlohmann::json& j, Item& item) {
    item.id           = j.value("id", std::string{});
    item.name         = j.value("name", std::string{});
    item.description  = j.value("description", std::string{});
    item.takeable     = j.value("takeable", true);
    item.key_item     = j.value("key_item", false);
    item.hidden       = j.value("hidden", false);
    item.unlock_target = j.value("unlock_target", std::string{});
    item.properties   = j.value("properties", std::map<std::string, std::string>{});
}

} // namespace chronicle
