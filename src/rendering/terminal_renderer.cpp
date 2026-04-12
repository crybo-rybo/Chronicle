#include "rendering/terminal_renderer.hpp"

namespace chronicle {

TerminalRenderer::TerminalRenderer(std::ostream &out, std::istream &in, bool use_color)
    : out_(out), in_(in), use_color_(use_color) {}

TerminalRenderer::TerminalRenderer(const std::vector<std::string> &npc_names, std::ostream &out,
                                   std::istream &in, bool use_color)
    : TerminalRenderer(out, in, use_color) {
    assign_npc_colors(npc_names);
}

void TerminalRenderer::assign_npc_colors(const std::vector<std::string> &npc_names) {
    // Sort a copy so color assignment is deterministic regardless of input order
    auto sorted = npc_names;
    std::sort(sorted.begin(), sorted.end());

    for (const auto &id : sorted) {
        if (npc_color_indices_.find(id) == npc_color_indices_.end()) {
            npc_color_indices_[id] = next_color_index_++;
        }
    }
}

std::string_view TerminalRenderer::npc_color(const std::string &npc_name) {
    auto it = npc_color_indices_.find(npc_name);
    if (it == npc_color_indices_.end()) {
        int idx = next_color_index_++;
        npc_color_indices_[npc_name] = idx;
        return kNpcPalette[static_cast<std::size_t>(idx % 6)];
    }
    return kNpcPalette[static_cast<std::size_t>(it->second % 6)];
}

void TerminalRenderer::write_colored(std::string_view color, std::string_view text) {
    if (use_color_) {
        out_ << color << text << kReset;
    } else {
        out_ << text;
    }
}

void TerminalRenderer::render_scene(const Location &loc, const World &world) {
    // Location name in bold
    out_ << "\n";
    write_colored(kBold, "--- " + std::string(loc.name) + " ---");
    out_ << "\n";

    // Description
    out_ << loc.describe(world) << "\n";

    // Exits
    if (!loc.exits.empty()) {
        out_ << "Exits: ";
        bool first = true;
        for (const auto &[direction, _] : loc.exits) {
            if (!first)
                out_ << ", ";
            out_ << direction;
            first = false;
        }
        out_ << "\n";
    }

    // Visible items
    bool has_visible_items = false;
    for (const auto &item_id : loc.items) {
        auto it = world.items.find(item_id);
        if (it != world.items.end() && !it->second.hidden) {
            if (!has_visible_items) {
                out_ << "You see: ";
                has_visible_items = true;
            } else {
                out_ << ", ";
            }
            out_ << it->second.name;
        }
    }
    if (has_visible_items)
        out_ << "\n";

    // NPCs present
    bool has_npcs = false;
    for (const auto &npc_id : loc.npcs) {
        auto it = world.npcs.find(npc_id);
        if (it != world.npcs.end()) {
            if (!has_npcs) {
                out_ << "Present: ";
                has_npcs = true;
            } else {
                out_ << ", ";
            }
            out_ << it->second.identity.name;
        }
    }
    if (has_npcs)
        out_ << "\n";
}

void TerminalRenderer::render_move(const std::string &direction,
                                   const std::string &new_location_name) {
    out_ << "You head " << direction << " to " << new_location_name << ".\n";
}

void TerminalRenderer::render_npc_intro(std::string_view npc_name, std::string_view mood) {
    write_colored(npc_color(std::string(npc_name)), npc_name);
    out_ << " seems " << mood << ".\n";
}

void TerminalRenderer::begin_npc_dialogue(std::string_view npc_name) {
    if (use_color_) {
        out_ << npc_color(std::string(npc_name));
        out_.flush();
    }
}

void TerminalRenderer::stream_token(std::string_view token) {
    out_ << token;
    out_.flush();
}

void TerminalRenderer::flush_dialogue() {
    if (use_color_) {
        out_ << kReset;
    }
    out_ << "\n";
}

void TerminalRenderer::render_action(std::string_view narration) {
    write_colored(kItalic, "* " + std::string(narration) + " *");
    out_ << "\n";
}

void TerminalRenderer::render_inventory(const Player &player, const World &world) {
    if (player.inventory.empty()) {
        out_ << "You are carrying nothing.\n";
        return;
    }
    out_ << "You are carrying:\n";
    for (const auto &item_id : player.inventory) {
        auto it = world.items.find(item_id);
        if (it != world.items.end()) {
            out_ << "  - " << it->second.name << "\n";
        }
    }
}

void TerminalRenderer::render_item_examine(const Item &item) {
    write_colored(kBold, item.name);
    out_ << "\n" << item.description << "\n";

    auto readable_it = item.properties.find("readable");
    if (readable_it != item.properties.end()) {
        auto text_it = item.properties.find("text");
        if (text_it != item.properties.end()) {
            out_ << text_it->second << "\n";
        }
    }
}

void TerminalRenderer::render_error(std::string_view message) {
    write_colored(kDim, "[" + std::string(message) + "]");
    out_ << "\n";
}

void TerminalRenderer::render_system(std::string_view message) {
    write_colored(kDim, message);
    out_ << "\n";
}

void TerminalRenderer::render_time_advance(const Clock &clock) {
    out_ << "\n";
    write_colored(kBoldCyan, "[" + clock.to_display_string() + "]");
    out_ << "\n\n";
}

void TerminalRenderer::render_resolution(std::string_view ending_narration) {
    out_ << "\n" << ending_narration << "\n";
}

std::string TerminalRenderer::get_player_input(std::string_view prompt) {
    out_ << prompt;
    out_.flush();
    std::string line;
    std::getline(in_, line);
    return line;
}

void TerminalRenderer::clear_input_line() {
    out_ << "\r" << std::string(80, ' ') << "\r";
}

} // namespace chronicle
