#include "chronicle/render.hpp"

#include <iostream>

namespace chronicle {

TerminalRenderer::TerminalRenderer() : out_(std::cout), in_(std::cin) {}

TerminalRenderer::TerminalRenderer(std::ostream &out, std::istream &in) : out_(out), in_(in) {}

void TerminalRenderer::print_events(const GameEvents &events) const {
    for (const auto &event : events) {
        if (event.text.empty()) {
            continue;
        }
        out_ << event.text << '\n';
        if (event.kind == EventKind::look || event.kind == EventKind::ending ||
            event.kind == EventKind::title) {
            out_ << '\n';
        }
    }
    out_.flush();
}

std::string TerminalRenderer::prompt(const std::string &prefix) const {
    out_ << prefix << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        return "quit";
    }
    return line;
}

} // namespace chronicle
