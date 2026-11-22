// Where Chronicle keeps a player's own files.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace chronicle {

// ~/.chronicle, or ./.chronicle when the host has no home directory.
[[nodiscard]] std::filesystem::path chronicle_home();

// The directory holding one cartridge's save slots. The host names the root with
// `root`; otherwise saves live in ~/.chronicle/saves, except that a cartridge
// already saved in ./saves, where Chronicle used to put every save, stays there.
// Nothing is created. Throws std::invalid_argument for an unsafe cartridge id.
[[nodiscard]] std::filesystem::path
save_directory(const std::string &cartridge_id,
               const std::optional<std::filesystem::path> &root = std::nullopt);

} // namespace chronicle
