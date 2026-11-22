// Small text helpers shared by the game implementation translation units.
#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

// The text on one line: whitespace runs become one space and other control
// characters are dropped. What a model writes goes through this before it joins
// text whose lines mean something, such as a prompt or the player's screen.
inline std::string single_line(const std::string &text) {
    std::string line;
    bool gap = false;
    for (const char ch : text) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte == ' ' || (byte >= '\t' && byte <= '\r')) {
            gap = true;
        } else if (byte >= 0x20 && byte != 0x7F) {
            if (gap && !line.empty()) {
                line += ' ';
            }
            gap = false;
            line += ch;
        }
    }
    return line;
}

// A line of speech as the player reads it. Models often write their own quotation
// marks around what is said, with gestures between, and those are left as they are.
inline std::string dialogue_line(const std::string &speaker, const std::string &speech) {
    const std::string line = single_line(speech);
    return speaker + ": " + (line.contains('"') ? line : "\"" + line + "\"");
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

inline std::string without_article(const std::string &text) {
    for (const std::string_view article : {"the ", "a ", "an "}) {
        if (text.starts_with(article)) {
            return strip(text.substr(article.size()));
        }
    }
    return text;
}

// True when the player's words are this thing's id or full name.
inline bool names(const std::string &text, const std::string &id, const std::string &name) {
    const std::string wanted = without_article(strip(lower(text)));
    return !wanted.empty() && (wanted == lower(id) || wanted == without_article(lower(name)));
}

// Something the player's words may refer to.
struct Noun {
    std::string id;
    std::string name;
};

// How well the words fit a noun, best first. What the player reads on the screen
// counts for more than an id, which may look like another thing's name.
enum class Fit { none, part_of_name, words_of_name, id, name_but_for_article, name };

[[nodiscard]] inline Fit fit_of(const std::string &text, const Noun &noun) {
    const std::string typed = strip(lower(text));
    const std::string wanted = without_article(typed);
    const std::string name = lower(noun.name);
    if (wanted.empty()) {
        return Fit::none;
    }
    if (typed == name) {
        return Fit::name;
    }
    if (wanted == without_article(name)) {
        return Fit::name_but_for_article;
    }
    if (wanted == lower(noun.id)) {
        return Fit::id;
    }
    const auto name_words = split_words(name);
    if (std::ranges::all_of(split_words(wanted), [&](const std::string &word) {
            return std::ranges::find(name_words, word) != name_words.end();
        })) {
        return Fit::words_of_name;
    }
    return name.contains(wanted) ? Fit::part_of_name : Fit::none;
}

// The nouns the words fit best. More than one means the words are ambiguous.
[[nodiscard]] inline std::vector<Noun> best_matches(const std::string &text,
                                                    const std::vector<Noun> &nouns) {
    std::vector<Noun> best;
    Fit best_fit = Fit::none;
    for (const auto &noun : nouns) {
        const Fit fit = fit_of(text, noun);
        if (fit > best_fit) {
            best_fit = fit;
            best.clear();
        }
        // Nothing the player could type tells two things of one name apart.
        const bool twin = std::ranges::any_of(
            best, [&](const Noun &other) { return lower(other.name) == lower(noun.name); });
        if (fit != Fit::none && fit == best_fit && !twin) {
            best.push_back(noun);
        }
    }
    return best;
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
