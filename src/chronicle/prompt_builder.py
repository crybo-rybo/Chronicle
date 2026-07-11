"""Prompt assembly for NPC turns."""

from __future__ import annotations

import json

from chronicle.cartridge.models import WorldState
from chronicle.tools import tools_for_policy
from chronicle.types import Message, MessageRole, TurnRequest


def build_npc_turn(world: WorldState, npc_id: str, player_text: str) -> TurnRequest:
    npc = world.npcs[npc_id]
    identity = npc.identity
    state = npc.state

    knowledge_lines: list[str] = []
    for fact_id in identity.knowledge:
        fact = world.facts.get(fact_id)
        if fact:
            knowledge_lines.append(f"- [{fact_id}] {fact.text}")

    rules = [
        "Stay in character.",
        "Use tools to act; do not invent facts that are not in your knowledge list.",
        "Only reveal secrets when trust is high enough and the secret is authored.",
        "Prefer the say tool for spoken dialogue.",
    ]

    static = "\n".join(
        [
            f"You are {identity.name}, {identity.role}.",
            f"Personality: {identity.personality_summary}",
            f"Backstory: {identity.backstory}",
            "Goals:",
            *[f"- {g}" for g in identity.goals],
            "Knowledge:",
            *(knowledge_lines or ["- (none)"]),
            "Rules:",
            *[f"- {r}" for r in rules],
        ]
    )

    secret_line = ""
    if identity.secret and state.trust_toward_player >= identity.trust_reveal_threshold:
        secret_line = f"Secret you may reveal carefully: {identity.secret}"

    memories = sorted(state.memories, key=lambda m: m.importance, reverse=True)
    memory_budget = world.config.max_memory_tokens
    memory_lines: list[str] = []
    used = 0
    for mem in memories:
        line = f"- ({mem.importance}) {mem.summary}"
        used += len(line.split())
        if used > memory_budget // 4:  # rough token proxy
            break
        memory_lines.append(line)

    loc = world.locations[state.current_location]
    visible_npcs = [
        n.identity.name
        for nid, n in world.npcs.items()
        if n.state.current_location == state.current_location and nid != npc_id
    ]
    visible_items = [
        world.items[iid].name
        for iid, owner in world.item_owners.items()
        if owner == "location"
        and world.item_locations.get(iid) == state.current_location
        and iid in world.items
        and not world.items[iid].hidden
    ]

    dynamic = "\n".join(
        [
            f"Time: {world.clock.period_name} (turn {world.clock.turns_elapsed})",
            f"Your location: {loc.name} — {loc.base_description}",
            f"Mood: {state.mood}",
            f"Trust toward player: {state.trust_toward_player}",
            secret_line,
            "Memories:",
            *(memory_lines or ["- (none)"]),
            f"Also here: {', '.join(visible_npcs) if visible_npcs else 'no one'}",
            f"Visible items: {', '.join(visible_items) if visible_items else 'none'}",
        ]
    ).strip()

    player_inv = [world.items[i].name for i in world.player.inventory if i in world.items]
    user_payload = json.dumps(
        {"player_said": player_text, "player_inventory": player_inv},
        ensure_ascii=False,
    )

    return TurnRequest(
        messages=[
            Message(role=MessageRole.SYSTEM, content=static),
            Message(role=MessageRole.SYSTEM, content=dynamic),
            Message(role=MessageRole.USER, content=user_payload),
        ],
        tools=tools_for_policy(identity.tool_policy),
    )
