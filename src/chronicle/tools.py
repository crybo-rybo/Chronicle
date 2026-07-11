"""NPC tool JSON schemas and helpers."""

from __future__ import annotations

from chronicle.cartridge.models import NPC_TOOLS, VALID_MOODS, ToolPolicy
from chronicle.types import ToolSpec

TOOL_DESCRIPTIONS: dict[str, str] = {
    "say": "Speak aloud to the player.",
    "give_item": "Give an item you hold to the player.",
    "take_item": "Take an item from the player.",
    "update_mood": "Change your mood.",
    "update_trust": "Adjust trust toward the player by a signed delta.",
    "move_self": "Move to another allowed location.",
    "reveal_knowledge": "Reveal an authored fact you know.",
    "remember": "Store a short memory about this conversation.",
    "set_flag": "Set an authored narrative flag.",
    "inspect_item": "Inspect an item and learn its description (no world change).",
}


def tools_for_policy(policy: ToolPolicy) -> list[ToolSpec]:
    allowed = [t for t in policy.allowed_tools if t in NPC_TOOLS]
    specs: list[ToolSpec] = []
    for name in allowed:
        specs.append(
            ToolSpec(
                name=name,
                description=TOOL_DESCRIPTIONS.get(name, name),
                parameters=_parameters_for(name),
            )
        )
    return specs


def _parameters_for(name: str) -> dict:
    if name == "say":
        return {
            "type": "object",
            "properties": {"text": {"type": "string"}},
            "required": ["text"],
        }
    if name in {"give_item", "take_item", "inspect_item"}:
        return {
            "type": "object",
            "properties": {"item_id": {"type": "string"}},
            "required": ["item_id"],
        }
    if name == "update_mood":
        return {
            "type": "object",
            "properties": {"mood": {"type": "string", "enum": sorted(VALID_MOODS)}},
            "required": ["mood"],
        }
    if name == "update_trust":
        return {
            "type": "object",
            "properties": {"delta": {"type": "integer"}},
            "required": ["delta"],
        }
    if name == "move_self":
        return {
            "type": "object",
            "properties": {"location_id": {"type": "string"}},
            "required": ["location_id"],
        }
    if name == "reveal_knowledge":
        return {
            "type": "object",
            "properties": {"fact_id": {"type": "string"}},
            "required": ["fact_id"],
        }
    if name == "remember":
        return {
            "type": "object",
            "properties": {
                "summary": {"type": "string"},
                "importance": {"type": "integer", "minimum": 1, "maximum": 10},
            },
            "required": ["summary"],
        }
    if name == "set_flag":
        return {
            "type": "object",
            "properties": {
                "flag_id": {"type": "string"},
                "value": {"type": "boolean"},
            },
            "required": ["flag_id", "value"],
        }
    return {"type": "object", "properties": {}}
