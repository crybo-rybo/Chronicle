// Typed domain calls accepted by Chronicle's NPC policy gate. The LLM adapter
// has reflected argument aggregates and converts them into these dependency-free
// domain values at the boundary.
#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace chronicle::tools {

enum class Mood { fearful, friendly, grieving, hostile, neutral, suspicious };

[[nodiscard]] std::string to_string(Mood mood);
[[nodiscard]] std::optional<Mood> mood_from_string(std::string_view value);

struct Say {
    static constexpr std::string_view name = "say";
    std::string text;
};

struct GiveItem {
    static constexpr std::string_view name = "give_item";
    std::string item_id;
};

struct TakeItem {
    static constexpr std::string_view name = "take_item";
    std::string item_id;
};

struct UpdateMood {
    static constexpr std::string_view name = "update_mood";
    Mood mood;
};

struct UpdateTrust {
    static constexpr std::string_view name = "update_trust";
    int delta;
};

struct MoveSelf {
    static constexpr std::string_view name = "move_self";
    std::string location_id;
};

struct RevealKnowledge {
    static constexpr std::string_view name = "reveal_knowledge";
    std::string fact_id;
};

struct Remember {
    static constexpr std::string_view name = "remember";
    std::string summary;
    int importance = 5;
};

struct SetFlag {
    static constexpr std::string_view name = "set_flag";
    std::string flag_id;
    bool value;
};

struct InspectItem {
    static constexpr std::string_view name = "inspect_item";
    std::string item_id;
};

using NpcToolCall = std::variant<Say, GiveItem, TakeItem, UpdateMood, UpdateTrust, MoveSelf,
                                 RevealKnowledge, Remember, SetFlag, InspectItem>;

inline constexpr std::array NPC_TOOL_NAMES{
    Say::name,      GiveItem::name,        TakeItem::name, UpdateMood::name, UpdateTrust::name,
    MoveSelf::name, RevealKnowledge::name, Remember::name, SetFlag::name,    InspectItem::name,
};

[[nodiscard]] constexpr bool is_known_tool(const std::string_view name) {
    return std::ranges::find(NPC_TOOL_NAMES, name) != NPC_TOOL_NAMES.end();
}

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
    return std::visit(
        [](const auto &typed_call) { return std::remove_cvref_t<decltype(typed_call)>::name; },
        call);
}

} // namespace chronicle::tools
