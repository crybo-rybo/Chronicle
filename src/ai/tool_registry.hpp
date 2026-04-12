#pragma once
#include "entities/world.hpp"
#include <map>
#include <string>
#include <variant>

namespace chronicle {

struct MutationRequest {
    enum class Type {
        GiveItemToPlayer,
        TakeItemFromPlayer,
        UpdateNpcMood,
        UpdateNpcTrust,
        MoveNpc,
        RevealKnowledge,
        AddMemory,
        SetFlag
    };

    Type type;
    std::string npc_id;
    std::map<std::string, std::string> params;
};

class ToolRegistry {
public:
    using ValidationResult = std::variant<MutationRequest, std::string>;

    explicit ToolRegistry(const World &world);

    ValidationResult validate_give_item(const std::string &npc_id,
                                        const std::string &item_id) const;
    ValidationResult validate_take_item(const std::string &npc_id,
                                        const std::string &item_id) const;
    ValidationResult validate_update_mood(const std::string &npc_id,
                                          const std::string &mood) const;
    ValidationResult validate_update_trust(const std::string &npc_id, int delta) const;
    ValidationResult validate_move_npc(const std::string &npc_id,
                                       const std::string &location_id) const;
    ValidationResult validate_reveal_knowledge(const std::string &npc_id,
                                                const std::string &fact_id) const;
    ValidationResult validate_add_memory(const std::string &npc_id, const std::string &summary,
                                         int importance) const;
    ValidationResult validate_set_flag(const std::string &flag_id, bool value) const;

private:
    const World &world_;

    bool npc_exists(const std::string &npc_id) const;
    bool npc_has_item(const std::string &npc_id, const std::string &item_id) const;
    bool player_has_item(const std::string &item_id) const;
    bool is_key_item(const std::string &item_id) const;
    bool item_exists(const std::string &item_id) const;
    bool location_exists(const std::string &location_id) const;
    bool flag_exists(const std::string &flag_id) const;
    bool is_valid_mood(const std::string &mood) const;
};

} // namespace chronicle
