// Shared test fixtures: bundled example paths, temp dirs, and a small
// synthetic world exercising every mechanic (locks, hidden items, secrets,
// events) that the minimal example doesn't cover.
#pragma once

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unistd.h>

#include "chronicle/cartridge/loader.hpp"

namespace chronicle::testing {

inline std::filesystem::path repo_dir() {
    return {CHRONICLE_REPO_DIR};
}

inline std::filesystem::path minimal_example() {
    return repo_dir() / "examples" / "minimal";
}

inline std::filesystem::path broken_wheel_example() {
    return repo_dir() / "examples" / "broken_wheel";
}

// A unique temp directory removed when the object goes out of scope.
class TempDir {
  public:
    explicit TempDir(const std::string &tag) {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("chronicle_test_" + tag + "_" + std::to_string(::getpid()) + "_" +
                 std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;

    [[nodiscard]] const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

// Two locations (garden locked behind a gate key), one NPC with a secret and
// full tool access, one fact, one flag, and a turn_ge scripted event.
inline WorldState make_test_world() {
    using nlohmann::json;
    const json manifest{
        {"id", "testworld"},
        {"name", "Test World"},
        {"version", "1.0.0"},
        {"chronicle_schema_version", 1},
        {"files",
         {{"config", "config.json"},
          {"world", "world.json"},
          {"npcs", "npcs.json"},
          {"facts", "facts.json"},
          {"flags", "flags.json"},
          {"events", "events.json"}}},
    };
    const json config{{"turns_per_period", 2}, {"total_periods", 3}};
    const json world{
        {"start_location", "hall"},
        {"locations",
         {{"hall",
           {{"name", "Hall"},
            {"base_description", "A dusty hall."},
            {"exits", {{"north", "garden"}}},
            {"items", json::array({"gate_key", "old_coin", "hidden_note"})},
            {"locked_exits", json::array({"north"})}}},
          {"garden",
           {{"name", "Garden"},
            {"base_description", "An overgrown garden."},
            {"exits", {{"south", "hall"}}},
            {"items", json::array()}}}}},
        {"items",
         {{"gate_key",
           {{"name", "Gate Key"},
            {"description", "A heavy iron key."},
            {"unlock_target", "garden"}}},
          {"old_coin",
           {{"name", "Old Coin"},
            {"description", "A worn coin."},
            {"properties", {{"readable", "true"}, {"text", "MDCCLX"}}}}},
          {"hidden_note",
           {{"name", "Hidden Note"}, {"description", "A folded note."}, {"hidden", true}}},
          {"keepsake", {{"name", "Keepsake"}, {"description", "A small locket."}}},
          {"statue",
           {{"name", "Statue"}, {"description", "Too heavy to lift."}, {"takeable", false}}}}},
    };
    const json npcs{
        {"npcs",
         {{"keeper",
           {{"identity",
             {{"name", "Keeper"},
              {"role", "Groundskeeper"},
              {"personality_summary", "Wary but fair."},
              {"backstory", "Has tended the hall for decades."},
              {"secret", "The garden hides a grave."},
              {"goals", json::array({"Protect the grounds"})},
              {"knowledge", json::array({"fact_gate"})},
              {"trust_reveal_threshold", 60},
              {"tool_policy",
               {{"allowed_tools", json::array({"say", "give_item", "take_item", "update_mood",
                                               "update_trust", "move_self", "reveal_knowledge",
                                               "remember", "set_flag", "inspect_item"})},
                {"allowed_locations", json::array({"hall", "garden"})}}}}},
            {"state", {{"current_location", "hall"}, {"inventory", json::array({"keepsake"})}}}}}}},
    };
    const json facts{
        {"facts", {{"fact_gate", {{"text", "The gate was locked after the accident."}}}}}};
    const json flags{{"flags", {{"gate_seen", {{"default", false}}}}}};
    const json events{
        {"events",
         {{"evt_second_turn",
           {{"conditions", json::array({{{"type", "turn_ge"}, {"args", json::array({"1"})}}})},
            {"actions", json::array({{{"type", "narrate"},
                                      {"params", {{"text", "A clock chimes somewhere."}}}}})}}}}}};
    // Place statue in no location so validation flags it as a warning; the
    // rest of the tests want a fully valid world, so put it in the hall.
    json placed_world = world;
    placed_world["locations"]["hall"]["items"].push_back("statue");
    return assemble_world(manifest, config, placed_world, npcs, facts, flags, events);
}

// Write a complete valid package to dir (for loader/validator/library tests).
inline void write_package(const std::filesystem::path &dir, nlohmann::json overrides = {}) {
    using nlohmann::json;
    std::filesystem::create_directories(dir);
    json scenario{
        {"id", "diskworld"},
        {"name", "Disk World"},
        {"version", "0.1.0"},
        {"chronicle_schema_version", 1},
        {"files",
         {{"config", "config.json"},
          {"world", "world.json"},
          {"npcs", "npcs.json"},
          {"facts", "facts.json"},
          {"flags", "flags.json"},
          {"events", "events.json"}}},
    };
    json world{
        {"start_location", "cell"},
        {"locations",
         {{"cell",
           {{"name", "Cell"}, {"base_description", "A bare cell."}, {"exits", json::object()}}}}},
        {"items", json::object()},
    };
    json npcs{{"npcs", json::object()}};
    json facts{{"facts", json::object()}};
    json flags{{"flags", json::object()}};
    json events{{"events", json::object()}};
    json config = json::object();
    for (auto &[key, value] : overrides.items()) {
        if (key == "scenario") {
            scenario.merge_patch(value);
        } else if (key == "world") {
            world = value;
        } else if (key == "npcs") {
            npcs = value;
        } else if (key == "config") {
            config = value;
        }
    }
    const auto write = [&dir](const char *name, const json &doc) {
        std::ofstream stream(dir / name);
        stream << doc.dump(2);
    };
    write("scenario.json", scenario);
    write("config.json", config);
    write("world.json", world);
    write("npcs.json", npcs);
    write("facts.json", facts);
    write("flags.json", flags);
    write("events.json", events);
}

} // namespace chronicle::testing
