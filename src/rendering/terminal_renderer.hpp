#pragma once
#include "rendering/renderer.hpp"
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace chronicle {

class TerminalRenderer : public Renderer {
  public:
    explicit TerminalRenderer(std::ostream &out = std::cout, std::istream &in = std::cin,
                              bool use_color = true);

    void render_scene(const Location &loc, const World &world) override;
    void render_move(const std::string &direction,
                     const std::string &new_location_name) override;
    void render_npc_intro(std::string_view npc_name, std::string_view mood) override;
    void stream_token(std::string_view token) override;
    void flush_dialogue() override;
    void render_action(std::string_view narration) override;
    void render_inventory(const Player &player, const World &world) override;
    void render_item_examine(const Item &item) override;
    void render_error(std::string_view message) override;
    void render_system(std::string_view message) override;
    void render_time_advance(const Clock &clock) override;
    void render_resolution(std::string_view ending_narration) override;
    std::string get_player_input(std::string_view prompt) override;
    void clear_input_line() override;

  private:
    std::ostream &out_;
    std::istream &in_;
    bool use_color_;
    std::unordered_map<std::string, int> npc_color_indices_;
    int next_color_index_ = 0;

    static constexpr std::string_view kReset = "\033[0m";
    static constexpr std::string_view kBold = "\033[1m";
    static constexpr std::string_view kItalic = "\033[3m";
    static constexpr std::string_view kDim = "\033[2m";
    static constexpr std::string_view kBoldCyan = "\033[1;36m";
    static constexpr std::string_view kSoftYellow = "\033[33m";

    static constexpr std::array<std::string_view, 6> kNpcPalette = {{
        "\033[1;33m", // bold yellow
        "\033[1;35m", // bold magenta
        "\033[1;36m", // bold cyan
        "\033[1;32m", // bold green
        "\033[1;34m", // bold blue
        "\033[1;31m", // bold red
    }};

    std::string_view npc_color(const std::string &npc_name);
    void write_colored(std::string_view color, std::string_view text);
};

} // namespace chronicle
