#include "entities/config.hpp"
#include <fstream>
#include <stdexcept>

namespace chronicle {

Config Config::load(const std::filesystem::path &path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Config: cannot open file: " + path.string());
    try {
        return nlohmann::json::parse(f).get<Config>();
    } catch (const nlohmann::json::exception &e) {
        throw std::runtime_error("Config: parse error in " + path.string() + ": " + e.what());
    }
}

void Config::save(const std::filesystem::path &path) const {
    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Config: cannot write file: " + path.string());
    nlohmann::json j = *this;
    f << j.dump(2) << '\n';
    if (!f.good())
        throw std::runtime_error("Config: write failed for " + path.string());
}

} // namespace chronicle
