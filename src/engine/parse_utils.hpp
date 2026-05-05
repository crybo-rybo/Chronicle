/**
 * @file parse_utils.hpp
 * @brief Small parsing utilities shared across engine and AI layers.
 */

#pragma once
#include <map>
#include <optional>
#include <string>

namespace chronicle {

/// @brief Look up a string parameter in a name→value map.
///
/// @details Returns @c std::nullopt if the key is missing or its value is empty,
/// so callers can treat both cases as "not provided".
inline std::optional<std::string>
param_value(const std::map<std::string, std::string> &params, const std::string &key) {
    auto it = params.find(key);
    if (it == params.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second;
}


/// @brief Strictly parse a boolean string.
///
/// @details Accepts only @c "true" / @c "1" (→ true) and @c "false" / @c "0"
/// (→ false).  Any other value returns @c std::nullopt.
inline std::optional<bool> parse_bool(const std::string &value) {
    if (value == "true" || value == "1") {
        return true;
    }
    if (value == "false" || value == "0") {
        return false;
    }
    return std::nullopt;
}

/// @brief Strictly parse an integer string.
///
/// @details Returns @c std::nullopt if the string is empty, contains trailing
/// non-digit characters, or is out of @c int range.
inline std::optional<int> parse_int(const std::string &value) {
    try {
        std::size_t parsed = 0;
        int result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

} // namespace chronicle
