// Typed domain calls accepted by Chronicle's NPC policy gate. The LLM adapter
// has reflected argument aggregates and converts them into these dependency-free
// domain values at the boundary.
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
    bool value;
};

struct InspectItem {
    std::string item_id;
};

using NpcToolCall = std::variant<Say, GiveItem, TakeItem, UpdateMood, UpdateTrust, MoveSelf,
                                 RevealKnowledge, Remember, SetFlag, InspectItem>;

// Registration name of the tool a call belongs to (e.g. "give_item").
[[nodiscard]] std::string_view tool_name(const NpcToolCall &call);

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

} // namespace chronicle::tools
