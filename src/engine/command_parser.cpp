/**
 * @file command_parser.cpp
 * @brief Implementation of @ref CommandParser — converts raw input to structured commands.
 *
 * @details Builds the verb lookup table from the config file (or the built-in
 * fallback table) and implements the phase-aware parsing logic including
 * direction expansion, alias resolution, and use-syntax parsing.
 */

#include "engine/command_parser.hpp"
#include "engine/text_utils.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace chronicle {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

struct AliasTables {
    std::unordered_map<std::string, CommandVerb> verb_table;
    std::unordered_map<std::string, std::string> command_aliases;
};

std::unordered_map<std::string, CommandVerb> fallback_verb_table() {
    return {
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
        {"look", CommandVerb::Look},
        {"l", CommandVerb::Look},
        {"examine", CommandVerb::Examine},
        {"x", CommandVerb::Examine},
        {"inspect", CommandVerb::Examine},
        {"take", CommandVerb::Take},
        {"get", CommandVerb::Take},
        {"grab", CommandVerb::Take},
        {"pick", CommandVerb::Take},
        {"drop", CommandVerb::Drop},
        {"use", CommandVerb::Use},
        {"give", CommandVerb::Give},
        {"talk", CommandVerb::Talk},
        {"speak", CommandVerb::Talk},
        {"chat", CommandVerb::Talk},
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
}

AliasTables load_alias_tables(const std::filesystem::path &config_path) {
    std::ifstream in(config_path);
    if (!in.is_open()) {
        return {.verb_table = fallback_verb_table()};
    }

    nlohmann::json config;
    try {
        config = nlohmann::json::parse(in);
    } catch (const nlohmann::json::exception &e) {
        throw std::runtime_error("CommandParser: parse error in " + config_path.string() + ": " +
                                 e.what());
    }

    auto aliases_it = config.find("verb_aliases");
    if (aliases_it == config.end()) {
        return {.verb_table = fallback_verb_table()};
    }
    if (!aliases_it->is_object()) {
        throw std::runtime_error("CommandParser: verb_aliases must be a JSON object");
    }

    AliasTables result{.verb_table = fallback_verb_table()};
    for (const auto &[alias_or_verb, target] : aliases_it->items()) {
        if (target.is_string()) {
            auto key = text::trim_and_lower(alias_or_verb);
            auto command = text::trim_copy(target.get<std::string>());
            if (!key.empty() && !command.empty()) {
                result.command_aliases[key] = std::move(command);
            }
            continue;
        }

        throw std::runtime_error("CommandParser: verb_aliases.'" + alias_or_verb +
                                 "' must be a string command");
    }

    return result;
}

bool is_hard_conversation_command(CommandVerb verb) {
    return verb == CommandVerb::Quit || verb == CommandVerb::Save || verb == CommandVerb::Load ||
           verb == CommandVerb::Help || verb == CommandVerb::Inventory || verb == CommandVerb::Look;
}

// Map directional shortcut to full direction name.
// Returns empty string if not a directional shortcut.
std::string expand_direction(const std::string &word) {
    static const std::unordered_map<std::string, std::string> dir_map = {
        {"n", "north"}, {"s", "south"}, {"e", "east"}, {"w", "west"}, {"u", "up"}, {"d", "down"},
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

CommandParser::CommandParser(std::filesystem::path config_path) {
    auto aliases = load_alias_tables(config_path);
    verb_table_ = std::move(aliases.verb_table);
    command_aliases_ = std::move(aliases.command_aliases);
}

// ---------------------------------------------------------------------------
// parse_use_syntax
// ---------------------------------------------------------------------------

ParsedCommand CommandParser::parse_use_syntax(const std::string &args_after_verb,
                                              const std::string &raw) const {
    ParsedCommand result;
    result.verb = CommandVerb::Use;
    result.raw_input = raw;

    std::string args = text::trim_copy(args_after_verb);

    // Lower-case once, outside the loop, to avoid redundant allocation per separator.
    std::string lower_args = text::to_lower_copy(args);

    // Look for " on " or " with " separators
    for (std::string_view sep : {" on ", " with "}) {
        auto pos = lower_args.find(sep);
        if (pos != std::string::npos) {
            result.primary_arg = text::trim_copy(args.substr(0, pos));
            result.secondary_arg = text::trim_copy(args.substr(pos + sep.size()));
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
    return parse_internal(raw_input, phase, raw_input, true);
}

ParsedCommand CommandParser::parse_internal(const std::string &input, GamePhase phase,
                                            const std::string &raw_input,
                                            bool expand_aliases) const {
    ParsedCommand result;
    result.raw_input = raw_input;

    std::string trimmed = text::trim_copy(input);
    if (trimmed.empty()) {
        result.verb = CommandVerb::Unknown;
        return result;
    }

    if (expand_aliases) {
        auto alias_it = command_aliases_.find(text::trim_and_lower(trimmed));
        if (alias_it != command_aliases_.end()) {
            return parse_internal(alias_it->second, phase, raw_input, false);
        }
    }

    // Split into first word and remainder
    std::string first_word;
    std::string remainder;
    auto space_pos = trimmed.find(' ');
    if (space_pos == std::string::npos) {
        first_word = trimmed;
    } else {
        first_word = trimmed.substr(0, space_pos);
        remainder = text::trim_copy(trimmed.substr(space_pos + 1));
    }

    std::string first_lower = text::to_lower_copy(first_word);
    auto verb_it = verb_table_.find(first_lower);

    // InConversation phase: only hard commands break out of dialogue
    if (phase == GamePhase::InConversation) {
        if (verb_it == verb_table_.end() || !is_hard_conversation_command(verb_it->second)) {
            result.verb = CommandVerb::Dialogue;
            result.primary_arg = trimmed;
            return result;
        }
    }

    // Look up verb
    if (verb_it == verb_table_.end()) {
        result.verb = CommandVerb::Unknown;
        result.primary_arg = first_lower;
        return result;
    }

    CommandVerb verb = verb_it->second;

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

    // Normalize direction arguments to lowercase world IDs
    if (verb == CommandVerb::Go && !result.primary_arg.empty()) {
        result.primary_arg = text::to_lower_copy(result.primary_arg);
    }

    return result;
}

} // namespace chronicle
