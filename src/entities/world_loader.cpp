/**
 * @file world_loader.cpp
 * @brief Implementation of @ref load_world — assembles a @ref World from JSON data files.
 *
 * @details Reads @c world.json (locations, items, start position), @c npcs.json
 * (NPC identity and initial state), @c facts.json, @c flags.json, and
 * @c events.json (scripted triggers).
 * Entity IDs are injected from JSON map keys before deserialisation, and NPCs
 * are cross-referenced into their starting location NPC lists.
 */

#include "entities/world_loader.hpp"
#include "entities/fact.hpp"
#include "entities/flag.hpp"
#include "entities/world_validator.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace chronicle {

World load_world(const WorldFileSet &files) {
    World world;

    // -----------------------------------------------------------------------
    // 1. Parse world.json — locations, items, player start
    // -----------------------------------------------------------------------
    {
        auto path = files.world;
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("load_world: cannot open " + path.string());
        }

        nlohmann::json j_world;
        try {
            j_world = nlohmann::json::parse(file);
        } catch (const nlohmann::json::parse_error &e) {
            throw std::runtime_error("load_world: failed to parse " + path.string() + ": " +
                                     e.what());
        }

        // Player start location
        world.player.current_location = j_world.at("start_location").get<std::string>();

        // Locations — inject "id" from map key before deserializing
        for (auto &[key, value] : j_world.at("locations").items()) {
            auto loc_json = value; // mutable copy
            loc_json["id"] = key;  // inject id from map key
            Location loc = loc_json.get<Location>();
            world.locations[key] = std::move(loc);
        }

        // Items — inject "id" from map key before deserializing
        for (auto &[key, value] : j_world.at("items").items()) {
            auto item_json = value; // mutable copy
            item_json["id"] = key;  // inject id from map key
            Item item = item_json.get<Item>();
            world.items[key] = std::move(item);
        }
    }

    // -----------------------------------------------------------------------
    // 2. Parse flags.json — narrative flag declarations
    // -----------------------------------------------------------------------
    {
        auto path = files.flags;
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("load_world: cannot open " + path.string());
        }

        nlohmann::json j_flags;
        try {
            j_flags = nlohmann::json::parse(file);
        } catch (const nlohmann::json::parse_error &e) {
            throw std::runtime_error("load_world: failed to parse " + path.string() + ": " +
                                     e.what());
        }
        if (j_flags.contains("flags")) {
            for (auto &[key, value] : j_flags.at("flags").items()) {
                Flag flag = value.get<Flag>();
                flag.id = key;
                world.flags[key] = flag.default_value;
            }
        }
    }

    // -----------------------------------------------------------------------
    // 3. Parse facts.json — authored fact registry
    // -----------------------------------------------------------------------
    {
        auto path = files.facts;
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("load_world: cannot open " + path.string());
        }

        nlohmann::json j_facts;
        try {
            j_facts = nlohmann::json::parse(file);
        } catch (const nlohmann::json::parse_error &e) {
            throw std::runtime_error("load_world: failed to parse " + path.string() + ": " +
                                     e.what());
        }
        if (j_facts.contains("facts")) {
            for (auto &[key, value] : j_facts.at("facts").items()) {
                Fact fact = value.get<Fact>();
                fact.id = key;
                world.facts[key] = std::move(fact);
            }
        }
    }

    // -----------------------------------------------------------------------
    // 4. Parse npcs.json — NPCs and cross-reference with locations
    // -----------------------------------------------------------------------
    {
        auto path = files.npcs;
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("load_world: cannot open " + path.string());
        }

        nlohmann::json j_npcs;
        try {
            j_npcs = nlohmann::json::parse(file);
        } catch (const nlohmann::json::parse_error &e) {
            throw std::runtime_error("load_world: failed to parse " + path.string() + ": " +
                                     e.what());
        }

        for (auto &[key, value] : j_npcs.at("npcs").items()) {
            Npc npc = value.get<Npc>();
            npc.identity.id = key; // ensure id matches map key
            // Capture location before move invalidates the local npc object.
            const auto start_location = npc.state.current_location;
            world.npcs[key] = std::move(npc);

            // Add NPC to their location's npcs vector
            if (auto loc_it = world.locations.find(start_location);
                loc_it != world.locations.end()) {
                loc_it->second.npcs.push_back(key);
            }
        }
    }

    // -----------------------------------------------------------------------
    // 5. Parse events.json — event triggers
    // -----------------------------------------------------------------------
    {
        auto path = files.events;
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("load_world: cannot open " + path.string());
        }

        nlohmann::json j_events;
        try {
            j_events = nlohmann::json::parse(file);
        } catch (const nlohmann::json::parse_error &e) {
            throw std::runtime_error("load_world: failed to parse " + path.string() + ": " +
                                     e.what());
        }

        for (auto &[key, value] : j_events.at("events").items()) {
            auto event_json = value; // mutable copy
            event_json["id"] = key;  // inject id from map key
            EventTrigger trigger = event_json.get<EventTrigger>();
            world.events.push_back(std::move(trigger));
        }
    }

    // 6. Clock is default-constructed (Day 1, Morning) — nothing to do.

    // -----------------------------------------------------------------------
    // 7. Validate world integrity
    // -----------------------------------------------------------------------
    auto report = validate_world(world);
    if (!report.ok) {
        std::string msg = "load_world: validation failed:\n";
        for (const auto &e : report.errors) {
            msg += "  - " + e + "\n";
        }
        throw std::runtime_error(msg);
    }

    return world;
}

} // namespace chronicle
