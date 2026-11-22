#include "chronicle/paths.hpp"

#include <cstdlib>
#include <stdexcept>

#include "chronicle/cartridge/validator.hpp"

namespace chronicle {

namespace fs = std::filesystem;

namespace {

bool holds_saves(const fs::path &directory) {
    std::error_code ec;
    if (!fs::is_directory(fs::symlink_status(directory, ec))) {
        return false;
    }
    fs::directory_iterator it(directory, ec);
    for (const fs::directory_iterator end; !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        if (name.starts_with("slot_") && name.ends_with(".json")) {
            return true;
        }
    }
    return false;
}

} // namespace

fs::path chronicle_home() {
    const char *home = std::getenv("HOME");
    const fs::path base = home != nullptr && *home != '\0' ? fs::path(home) : fs::current_path();
    return base / ".chronicle";
}

fs::path save_directory(const std::string &cartridge_id, const std::optional<fs::path> &root) {
    if (!is_safe_cartridge_id(cartridge_id)) {
        throw std::invalid_argument("Unsafe cartridge id cannot be used for saves: " +
                                    cartridge_id);
    }
    if (root) {
        return *root / cartridge_id;
    }
    std::error_code ec;
    const auto legacy = fs::current_path(ec) / "saves" / cartridge_id;
    if (!ec && holds_saves(legacy)) {
        return legacy;
    }
    return chronicle_home() / "saves" / cartridge_id;
}

} // namespace chronicle
