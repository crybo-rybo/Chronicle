/**
 * @file text_utils.hpp
 * @brief Lightweight string manipulation utilities for Chronicle.
 *
 * @details All functions in @c chronicle::text are header-only and allocation-
 * friendly.  They are used throughout the engine and parser for normalising
 * player input before comparison or storage.
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace chronicle::text {

/// @brief Return a copy of @p input with leading and trailing whitespace removed.
///
/// @details Strips @c ' ', @c '\\t', @c '\\n', and @c '\\r'.  Returns an empty
/// string if @p input consists entirely of whitespace.
///
/// @param input The string view to trim.
/// @return A new @c std::string without surrounding whitespace.
inline std::string trim_copy(std::string_view input) {
    const auto begin = input.find_first_not_of(" \t\n\r");
    if (begin == std::string_view::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\n\r");
    return std::string(input.substr(begin, end - begin + 1));
}

/// @brief Return a copy of @p input converted to lowercase.
///
/// @details Uses @c std::tolower through an @c unsigned @c char cast to avoid
/// undefined behaviour on negative @c char values.
///
/// @param input The string view to convert.
/// @return A new @c std::string with all ASCII characters lower-cased.
inline std::string to_lower_copy(std::string_view input) {
    std::string result(input);
    std::ranges::transform(result, result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

/// @brief Case-insensitive substring search.
///
/// @details Both @p haystack and @p needle are lower-cased before the search,
/// so the comparison is locale-independent ASCII fold.
///
/// @param haystack The string to search within.
/// @param needle   The substring to look for.  An empty needle always returns @c false.
/// @return @c true if the lower-cased @p needle appears anywhere in the
///         lower-cased @p haystack.
inline bool contains_normalized(std::string_view haystack, std::string_view needle) {
    auto haystack_lower = to_lower_copy(haystack);
    auto needle_lower = to_lower_copy(needle);
    return !needle_lower.empty() && haystack_lower.find(needle_lower) != std::string::npos;
}

} // namespace chronicle::text
