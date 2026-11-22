// Terminal renderer — keep it dumb.
#pragma once

#include <iosfwd>
#include <optional>
#include <string>

#include "chronicle/types.hpp"

namespace chronicle {

// Text that cannot drive the terminal it is printed on: cartridges and models write
// what Chronicle prints, and an escape sequence can move the cursor, hide text, or
// set the clipboard. Newlines and tabs stay.
[[nodiscard]] std::string printable(const std::string &text);

class TerminalRenderer {
  public:
    TerminalRenderer();
    TerminalRenderer(std::ostream &out, std::istream &in);

    void print_events(const GameEvents &events) const;

    // Show the prompt and read a line; nothing at the end of the input.
    [[nodiscard]] std::optional<std::string> prompt(const std::string &prefix = "> ") const;

  private:
    std::ostream &out_;
    std::istream &in_;
};

} // namespace chronicle
