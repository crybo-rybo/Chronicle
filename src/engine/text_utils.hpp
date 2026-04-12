#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace chronicle::text {

inline std::string trim_copy(std::string_view input) {
    const auto begin = input.find_first_not_of(" \t\n\r");
    if (begin == std::string_view::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\n\r");
    return std::string(input.substr(begin, end - begin + 1));
}

inline std::string to_lower_copy(std::string_view input) {
    std::string result(input);
    std::ranges::transform(result, result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

inline bool contains_normalized(std::string_view haystack, std::string_view needle) {
    auto haystack_lower = to_lower_copy(haystack);
    auto needle_lower = to_lower_copy(needle);
    return !needle_lower.empty() && haystack_lower.find(needle_lower) != std::string::npos;
}

} // namespace chronicle::text
