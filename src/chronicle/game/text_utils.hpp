// Small text helpers shared by the game implementation translation units.
#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace chronicle::game_detail {

inline std::string lower(std::string text) {
    std::ranges::transform(text, text.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

inline std::string strip(const std::string &text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

inline std::vector<std::string> split_words(const std::string &text) {
    std::istringstream stream(text);
    std::vector<std::string> words;
    for (std::string word; stream >> word;) {
        words.push_back(std::move(word));
    }
    return words;
}

// The normalized first word and the original-case remainder after it.
inline std::pair<std::string, std::string> split_command(const std::string &text) {
    const auto stripped = strip(text);
    const auto space = stripped.find_first_of(" \t");
    if (space == std::string::npos) {
        return {lower(stripped), ""};
    }
    return {lower(stripped.substr(0, space)), strip(stripped.substr(space + 1))};
}

inline bool contains(const std::vector<std::string> &values, const std::string &value) {
    return std::ranges::find(values, value) != values.end();
}

inline std::optional<int> parse_int(const std::string &text) {
    int value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

inline std::string format_template(std::string text,
                                   const std::map<std::string, std::string> &arguments) {
    for (const auto &[key, value] : arguments) {
        const std::string placeholder = "{" + key + "}";
        std::size_t position = 0;
        while ((position = text.find(placeholder, position)) != std::string::npos) {
            text.replace(position, placeholder.size(), value);
            position += value.size();
        }
    }
    return text;
}

} // namespace chronicle::game_detail
