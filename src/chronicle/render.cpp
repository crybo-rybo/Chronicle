#include "chronicle/render.hpp"

#include <algorithm>
#include <iostream>

namespace chronicle {

namespace {

// The length of the well-formed UTF-8 sequence at `at`, or 0.
std::size_t utf8_length(const std::string &text, const std::size_t at) {
    const auto lead = static_cast<unsigned char>(text[at]);
    const std::size_t length = lead < 0x80                   ? 1
                               : lead >= 0xC2 && lead < 0xE0 ? 2
                               : lead >= 0xE0 && lead < 0xF0 ? 3
                               : lead >= 0xF0 && lead < 0xF5 ? 4
                                                             : 0;
    if (length == 0 || at + length > text.size()) {
        return 0;
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
        if ((static_cast<unsigned char>(text[at + offset]) & 0xC0U) != 0x80U) {
            return 0;
        }
    }
    return length;
}

} // namespace

std::string printable(const std::string &text) {
    std::string safe;
    safe.reserve(text.size());
    for (std::size_t at = 0; at < text.size();) {
        const auto byte = static_cast<unsigned char>(text[at]);
        const auto length = utf8_length(text, at);
        const bool control = length == 0 || (byte < 0x20 && byte != '\n' && byte != '\t') ||
                             byte == 0x7F ||
                             // U+0080 to U+009F: the one-character forms of escape sequences.
                             (byte == 0xC2 && static_cast<unsigned char>(text[at + 1]) < 0xA0);
        if (byte == '\r' && at + 1 < text.size() && text[at + 1] == '\n') {
            ++at; // A line end written on Windows.
            continue;
        }
        if (control) {
            safe += '?';
        } else {
            safe.append(text, at, length);
        }
        at += std::max<std::size_t>(length, 1);
    }
    return safe;
}

TerminalRenderer::TerminalRenderer() : out_(std::cout), in_(std::cin) {}

TerminalRenderer::TerminalRenderer(std::ostream &out, std::istream &in) : out_(out), in_(in) {}

void TerminalRenderer::print_events(const GameEvents &events) const {
    for (const auto &event : events) {
        if (event.text.empty()) {
            continue;
        }
        out_ << printable(event.text) << '\n';
        if (event.kind == EventKind::look || event.kind == EventKind::ending ||
            event.kind == EventKind::title) {
            out_ << '\n';
        }
    }
    out_.flush();
}

std::optional<std::string> TerminalRenderer::prompt(const std::string &prefix) const {
    out_ << prefix << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        return std::nullopt;
    }
    return line;
}

} // namespace chronicle
