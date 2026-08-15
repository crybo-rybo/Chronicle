// The NPC tool palette. These plain aggregates are the single source of truth
// for tool arguments: the game gate consumes them directly, and the LLM layer
// derives each tool's JSON schema from them via C++26 reflection (scry).
// Member names and defaults are therefore part of the cartridge-facing
// contract — see docs/console-api.md.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace chronicle::tools {

enum class Mood { fearful, friendly, grieving, hostile, neutral, suspicious };

[[nodiscard]] std::string to_string(Mood mood);
[[nodiscard]] std::optional<Mood> mood_from_string(std::string_view value);

struct Say {
    std::string text;
};

struct GiveItem {
    std::string item_id;
};

struct TakeItem {
    std::string item_id;
};

struct UpdateMood {
    Mood mood;
};

struct UpdateTrust {
    int delta;
};

struct MoveSelf {
    std::string location_id;
};

struct RevealKnowledge {
    std::string fact_id;
};

struct Remember {
    std::string summary;
    int importance = 5;
};

struct SetFlag {
    std::string flag_id;
    bool value = true;
};

struct InspectItem {
    std::string item_id;
};

using NpcToolCall = std::variant<Say, GiveItem, TakeItem, UpdateMood, UpdateTrust, MoveSelf,
                                 RevealKnowledge, Remember, SetFlag, InspectItem>;

// Registration name of the tool a call belongs to (e.g. "give_item").
[[nodiscard]] std::string_view tool_name(const NpcToolCall &call);

// Model-facing description for a tool name; empty view when unknown.
[[nodiscard]] std::string_view tool_description(std::string_view name);

inline std::string to_string(const Mood mood) {
    switch (mood) {
    case Mood::fearful:
        return "fearful";
    case Mood::friendly:
        return "friendly";
    case Mood::grieving:
        return "grieving";
    case Mood::hostile:
        return "hostile";
    case Mood::neutral:
        return "neutral";
    case Mood::suspicious:
        return "suspicious";
    }
    return "neutral";
}

inline std::optional<Mood> mood_from_string(const std::string_view value) {
    if (value == "fearful") {
        return Mood::fearful;
    }
    if (value == "friendly") {
        return Mood::friendly;
    }
    if (value == "grieving") {
        return Mood::grieving;
    }
    if (value == "hostile") {
        return Mood::hostile;
    }
    if (value == "neutral") {
        return Mood::neutral;
    }
    if (value == "suspicious") {
        return Mood::suspicious;
    }
    return std::nullopt;
}

inline std::string_view tool_name(const NpcToolCall &call) {
    struct Visitor {
        std::string_view operator()(const Say &) const { return "say"; }
        std::string_view operator()(const GiveItem &) const { return "give_item"; }
        std::string_view operator()(const TakeItem &) const { return "take_item"; }
        std::string_view operator()(const UpdateMood &) const { return "update_mood"; }
        std::string_view operator()(const UpdateTrust &) const { return "update_trust"; }
        std::string_view operator()(const MoveSelf &) const { return "move_self"; }
        std::string_view operator()(const RevealKnowledge &) const { return "reveal_knowledge"; }
        std::string_view operator()(const Remember &) const { return "remember"; }
        std::string_view operator()(const SetFlag &) const { return "set_flag"; }
        std::string_view operator()(const InspectItem &) const { return "inspect_item"; }
    };
    return std::visit(Visitor{}, call);
}

inline std::string_view tool_description(const std::string_view name) {
    if (name == "say") {
        return "Speak aloud to the player.";
    }
    if (name == "give_item") {
        return "Give an item you hold to the player.";
    }
    if (name == "take_item") {
        return "Take an item from the player.";
    }
    if (name == "update_mood") {
        return "Change your mood.";
    }
    if (name == "update_trust") {
        return "Adjust trust toward the player by a signed delta.";
    }
    if (name == "move_self") {
        return "Move to another allowed location.";
    }
    if (name == "reveal_knowledge") {
        return "Reveal an authored fact you know.";
    }
    if (name == "remember") {
        return "Store a short memory about this conversation.";
    }
    if (name == "set_flag") {
        return "Set an authored narrative flag.";
    }
    if (name == "inspect_item") {
        return "Inspect an item and learn its description (no world change).";
    }
    return {};
}

} // namespace chronicle::tools
