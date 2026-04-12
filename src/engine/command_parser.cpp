#include "engine/command_parser.hpp"
#include <algorithm>
#include <unordered_set>

namespace chronicle {

// ---------------------------------------------------------------------------
// Verb table — canonical verbs and aliases
// ---------------------------------------------------------------------------

const std::unordered_map<std::string, CommandVerb> CommandParser::verb_table_ = {
    // Movement
    {"go", CommandVerb::Go},
    {"walk", CommandVerb::Go},
    {"move", CommandVerb::Go},
    {"north", CommandVerb::Go},
    {"south", CommandVerb::Go},
    {"east", CommandVerb::Go},
    {"west", CommandVerb::Go},
    {"up", CommandVerb::Go},
    {"down", CommandVerb::Go},
    {"n", CommandVerb::Go},
    {"s", CommandVerb::Go},
    {"e", CommandVerb::Go},
    {"w", CommandVerb::Go},
    {"u", CommandVerb::Go},
    {"d", CommandVerb::Go},

    // Observation
    {"look", CommandVerb::Look},
    {"l", CommandVerb::Look},
    {"examine", CommandVerb::Examine},
    {"x", CommandVerb::Examine},
    {"inspect", CommandVerb::Examine},

    // Item manipulation
    {"take", CommandVerb::Take},
    {"get", CommandVerb::Take},
    {"grab", CommandVerb::Take},
    {"pick", CommandVerb::Take},
    {"drop", CommandVerb::Drop},
    {"use", CommandVerb::Use},
    {"give", CommandVerb::Give},

    // Social
    {"talk", CommandVerb::Talk},
    {"speak", CommandVerb::Talk},
    {"chat", CommandVerb::Talk},

    // Meta
    {"inventory", CommandVerb::Inventory},
    {"i", CommandVerb::Inventory},
    {"inv", CommandVerb::Inventory},
    {"save", CommandVerb::Save},
    {"load", CommandVerb::Load},
    {"quit", CommandVerb::Quit},
    {"exit", CommandVerb::Quit},
    {"q", CommandVerb::Quit},
    {"help", CommandVerb::Help},
    {"?", CommandVerb::Help},
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string to_lower(const std::string &s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Map directional shortcut to full direction name.
// Returns empty string if not a directional shortcut.
std::string expand_direction(const std::string &word) {
    static const std::unordered_map<std::string, std::string> dir_map = {
        {"n", "north"}, {"s", "south"}, {"e", "east"}, {"w", "west"},
        {"u", "up"},    {"d", "down"},
    };
    auto it = dir_map.find(word);
    if (it != dir_map.end())
        return it->second;
    return "";
}

// Words that are directional (both short and full forms).
bool is_direction_word(const std::string &word) {
    static const std::unordered_set<std::string> directions = {
        "n", "s", "e", "w", "u", "d", "north", "south", "east", "west", "up", "down",
    };
    return directions.contains(word);
}

} // namespace

// ---------------------------------------------------------------------------
// parse_use_syntax
// ---------------------------------------------------------------------------

ParsedCommand CommandParser::parse_use_syntax(const std::string &args_after_verb,
                                              const std::string &raw) const {
    ParsedCommand result;
    result.verb = CommandVerb::Use;
    result.raw_input = raw;

    std::string args = trim(args_after_verb);

    // Look for " on " or " with " separators
    for (const auto &sep : {" on ", " with "}) {
        std::string lower_args = to_lower(args);
        auto pos = lower_args.find(sep);
        if (pos != std::string::npos) {
            result.primary_arg = trim(args.substr(0, pos));
            result.secondary_arg = trim(args.substr(pos + std::string(sep).size()));
            return result;
        }
    }

    result.primary_arg = args;
    return result;
}

// ---------------------------------------------------------------------------
// parse
// ---------------------------------------------------------------------------

ParsedCommand CommandParser::parse(const std::string &raw_input, GamePhase phase) const {
    ParsedCommand result;
    result.raw_input = raw_input;

    std::string trimmed = trim(raw_input);
    if (trimmed.empty()) {
        result.verb = CommandVerb::Unknown;
        return result;
    }

    // Split into first word and remainder
    std::string first_word;
    std::string remainder;
    auto space_pos = trimmed.find(' ');
    if (space_pos == std::string::npos) {
        first_word = trimmed;
    } else {
        first_word = trimmed.substr(0, space_pos);
        remainder = trim(trimmed.substr(space_pos + 1));
    }

    std::string first_lower = to_lower(first_word);

    // InConversation phase: only hard commands break out of dialogue
    if (phase == GamePhase::InConversation) {
        static const std::unordered_set<std::string> hard_commands = {
            "quit", "exit", "q", "save", "load", "help", "?",
            "inventory", "i", "inv", "look", "l",
        };
        if (!hard_commands.contains(first_lower)) {
            result.verb = CommandVerb::Dialogue;
            return result;
        }
    }

    // Look up verb
    auto it = verb_table_.find(first_lower);
    if (it == verb_table_.end()) {
        result.verb = CommandVerb::Unknown;
        result.primary_arg = first_lower;
        return result;
    }

    CommandVerb verb = it->second;

    // Use verb has special syntax
    if (verb == CommandVerb::Use) {
        return parse_use_syntax(remainder, raw_input);
    }

    result.verb = verb;

    // Directional words used standalone (e.g. "n", "north")
    if (verb == CommandVerb::Go && is_direction_word(first_lower)) {
        std::string expanded = expand_direction(first_lower);
        if (!expanded.empty()) {
            result.primary_arg = expanded;
        } else {
            // Already a full direction name
            result.primary_arg = first_lower;
        }
        // If there was a remainder after a standalone direction, append it
        // (unusual but preserve it)
        if (!remainder.empty()) {
            result.primary_arg = remainder;
        }
        return result;
    }

    result.primary_arg = remainder;
    return result;
}

} // namespace chronicle
