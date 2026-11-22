// JSON that comes from outside the process: cartridges and save files.
#pragma once

#include <cstddef>
#include <nlohmann/json.hpp>
#include <string_view>

namespace chronicle {

// Far deeper than any cartridge goes.
inline constexpr std::size_t MAX_CARTRIDGE_JSON_DEPTH = 64;
// A save holds conversations, and a model's tool arguments may nest as deeply as
// Scry allows, which is 251 levels.
inline constexpr std::size_t MAX_SAVE_JSON_DEPTH = 512;

// Parse untrusted text. nlohmann::json parses any depth, but copying, comparing,
// and writing a value recurse, so deep enough input would overflow the stack
// later. It also keeps the last of two equal keys without a word, which hides half
// of what an author wrote. Both are refused here with std::invalid_argument;
// malformed JSON throws nlohmann::json::parse_error.
[[nodiscard]] nlohmann::json parse_untrusted_json(std::string_view text, std::size_t max_depth);

} // namespace chronicle
