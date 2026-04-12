#pragma once
#include "engine/game_phase.hpp"
#include <string>
#include <unordered_map>

namespace chronicle {

enum class CommandVerb {
    Go,
    Look,
    Examine,
    Take,
    Drop,
    Use,
    Give,
    Talk,
    Inventory,
    Save,
    Load,
    Quit,
    Help,
    Dialogue,
    Unknown
};

struct ParsedCommand {
    CommandVerb verb = CommandVerb::Unknown;
    std::string primary_arg;
    std::string secondary_arg;
    std::string raw_input;
};

class CommandParser {
  public:
    ParsedCommand parse(const std::string &raw_input, GamePhase phase) const;

  private:
    static const std::unordered_map<std::string, CommandVerb> verb_table_;
    ParsedCommand parse_use_syntax(const std::string &args_after_verb,
                                   const std::string &raw) const;
};

} // namespace chronicle
