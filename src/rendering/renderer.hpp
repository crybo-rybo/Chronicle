#pragma once
#include "entities/clock.hpp"
#include "entities/item.hpp"
#include "entities/location.hpp"
#include "entities/player.hpp"
#include "entities/world.hpp"
#include <string>
#include <string_view>

namespace chronicle {

class Renderer {
  public:
    virtual void render_scene(const Location &loc, const World &world) = 0;
    virtual void render_move(const std::string &direction,
                             const std::string &new_location_name) = 0;
    virtual void render_npc_intro(std::string_view npc_name, std::string_view mood) = 0;
    virtual void stream_token(std::string_view token) = 0;
    virtual void flush_dialogue() = 0;
    virtual void render_action(std::string_view narration) = 0;
    virtual void render_inventory(const Player &player, const World &world) = 0;
    virtual void render_item_examine(const Item &item) = 0;
    virtual void render_error(std::string_view message) = 0;
    virtual void render_system(std::string_view message) = 0;
    virtual void render_time_advance(const Clock &clock) = 0;
    virtual void render_resolution(std::string_view ending_narration) = 0;
    virtual std::string get_player_input(std::string_view prompt) = 0;
    virtual void clear_input_line() = 0;
    virtual ~Renderer() = default;
};

} // namespace chronicle
