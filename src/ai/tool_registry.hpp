#pragma once
#include "entities/world.hpp"
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace zoo {
class Agent;
} // namespace zoo

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

    // --- Read-only validation (unchanged) ---

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

    // --- Register methods: validate + enqueue on success ---
    // Return nullopt on success, error string on validation failure.

    std::optional<std::string> register_give_item(const std::string &npc_id,
                                                   const std::string &item_id);
    std::optional<std::string> register_take_item(const std::string &npc_id,
                                                   const std::string &item_id);
    std::optional<std::string> register_update_mood(const std::string &npc_id,
                                                     const std::string &mood);
    std::optional<std::string> register_update_trust(const std::string &npc_id, int delta);
    std::optional<std::string> register_move_npc(const std::string &npc_id,
                                                  const std::string &location_id);
    std::optional<std::string> register_reveal_knowledge(const std::string &npc_id,
                                                          const std::string &fact_id);
    std::optional<std::string> register_add_memory(const std::string &npc_id,
                                                    const std::string &summary, int importance);
    std::optional<std::string> register_set_flag(const std::string &flag_id, bool value);

    /// Captures dialogue text. Non-mutating — does NOT push to pending queue.
    void register_say(const std::string &npc_id, const std::string &dialogue);
    std::vector<std::pair<std::string, std::string>> drain_dialogue_log();

    /// Read-only access to the pending mutation queue.
    const std::vector<MutationRequest> &pending_mutations() const;

    /// Clears all pending mutations.
    void clear_pending();
    void clear_dialogue_log();
    void clear_all();

    /// Sets the NPC context used by registered zoo tool callbacks.
    void set_active_npc_id(std::string npc_id);

    /// Registers the available tools on the given zoo::Agent instance.
    void register_tools(zoo::Agent& agent, const std::string& npc_id);

private:
    const World &world_;
    std::vector<MutationRequest> pending_;
    std::vector<std::pair<std::string, std::string>> dialogue_log_;
    std::string active_npc_id_;

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
