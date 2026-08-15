// Terminal renderer — keep it dumb.
#pragma once

#include <iosfwd>
#include <string>

#include "chronicle/types.hpp"

namespace chronicle {

class TerminalRenderer {
  public:
    TerminalRenderer();
    TerminalRenderer(std::ostream &out, std::istream &in);

    void print_events(const GameEvents &events) const;

    // Show the prompt and read a line; EOF yields "quit".
    [[nodiscard]] std::string prompt(const std::string &prefix = "> ") const;

  private:
    std::ostream &out_;
    std::istream &in_;
};

} // namespace chronicle
