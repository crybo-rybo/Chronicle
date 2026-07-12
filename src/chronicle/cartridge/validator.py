"""Structural validation for scenario packages."""

from __future__ import annotations

from pathlib import Path

from chronicle.cartridge.loader import CartridgeError, load_package
from chronicle.cartridge.models import NPC_TOOLS, SCHEMA_VERSION, VALID_MOODS, WorldState


class ValidationIssue:
    def __init__(self, message: str, *, level: str = "error") -> None:
        self.message = message
        self.level = level

    def __str__(self) -> str:
        return f"[{self.level}] {self.message}"


def validate_world(world: WorldState) -> list[ValidationIssue]:
    issues: list[ValidationIssue] = []

    if world.manifest.chronicle_schema_version != SCHEMA_VERSION:
        issues.append(
            ValidationIssue(
                f"Unsupported chronicle_schema_version "
                f"{world.manifest.chronicle_schema_version} (expected {SCHEMA_VERSION})"
            )
        )

    if not world.manifest.id.strip():
        issues.append(ValidationIssue("scenario id must be non-empty"))

    if world.player.current_location not in world.locations:
        issues.append(ValidationIssue(f"start_location unknown: {world.player.current_location}"))

    for loc_id, loc in world.locations.items():
        for direction, dest in loc.exits.items():
            if dest not in world.locations:
                issues.append(
                    ValidationIssue(f"location {loc_id}: exit {direction} -> unknown {dest}")
                )
        for item_id in loc.items:
            if item_id not in world.items:
                issues.append(ValidationIssue(f"location {loc_id}: unknown item {item_id}"))

    for item_id in world.items:
        owner = world.item_owners.get(item_id)
        if owner is None:
            issues.append(
                ValidationIssue(f"item {item_id} is not placed anywhere", level="warning")
            )

    for npc_id, npc in world.npcs.items():
        if npc.state.current_location not in world.locations:
            issues.append(
                ValidationIssue(f"npc {npc_id}: unknown location {npc.state.current_location}")
            )
        if npc.state.mood not in VALID_MOODS:
            issues.append(ValidationIssue(f"npc {npc_id}: invalid mood {npc.state.mood}"))
        for fact_id in npc.identity.knowledge:
            if fact_id not in world.facts:
                issues.append(ValidationIssue(f"npc {npc_id}: unknown knowledge fact {fact_id}"))
        for item_id in npc.state.inventory:
            if item_id not in world.items:
                issues.append(ValidationIssue(f"npc {npc_id}: unknown inventory item {item_id}"))
        policy = npc.identity.tool_policy
        for tool in policy.allowed_tools:
            if tool not in NPC_TOOLS:
                issues.append(ValidationIssue(f"npc {npc_id}: unknown tool {tool}"))
        for item_id in policy.allowed_items:
            if item_id not in world.items:
                issues.append(ValidationIssue(f"npc {npc_id}: allowed_items unknown {item_id}"))
        for fact_id in policy.allowed_facts:
            if fact_id not in world.facts:
                issues.append(ValidationIssue(f"npc {npc_id}: allowed_facts unknown {fact_id}"))
        for flag_id in policy.allowed_flags:
            if flag_id not in world.flags and flag_id not in world.flag_meta:
                issues.append(ValidationIssue(f"npc {npc_id}: allowed_flags unknown {flag_id}"))
        for loc_id in policy.allowed_locations:
            if loc_id not in world.locations:
                issues.append(ValidationIssue(f"npc {npc_id}: allowed_locations unknown {loc_id}"))

    for event_id, event in world.events.items():
        for cond in event.conditions:
            _validate_condition(world, event_id, cond.type, cond.args, issues)
        for action in event.actions:
            _validate_event_action(world, event_id, action.type, action.params, issues)

    return issues


def _validate_condition(
    world: WorldState,
    event_id: str,
    ctype: str,
    args: list[str],
    issues: list[ValidationIssue],
) -> None:
    prefix = f"event {event_id} condition {ctype}"
    if ctype == "clock_is":
        if len(args) != 1:
            issues.append(ValidationIssue(f"{prefix}: expected 1 arg"))
    elif ctype == "player_at":
        if len(args) != 1 or args[0] not in world.locations:
            issues.append(ValidationIssue(f"{prefix}: bad location"))
    elif ctype == "flag_set":
        if len(args) != 2 or args[0] not in world.flag_meta and args[0] not in world.flags:
            issues.append(ValidationIssue(f"{prefix}: bad flag"))
    elif ctype == "npc_trust_ge":
        if len(args) != 2 or args[0] not in world.npcs:
            issues.append(ValidationIssue(f"{prefix}: bad npc/threshold"))
    elif ctype == "npc_at":
        if len(args) != 2 or args[0] not in world.npcs or args[1] not in world.locations:
            issues.append(ValidationIssue(f"{prefix}: bad npc/location"))
    elif ctype == "item_in_player_inv":
        if len(args) != 1 or args[0] not in world.items:
            issues.append(ValidationIssue(f"{prefix}: bad item"))
    elif ctype == "turn_ge":
        if len(args) != 1:
            issues.append(ValidationIssue(f"{prefix}: expected turn count"))
        else:
            try:
                int(args[0])
            except ValueError:
                issues.append(ValidationIssue(f"{prefix}: turn count not an int"))
    else:
        issues.append(ValidationIssue(f"{prefix}: unknown condition type"))


def _validate_event_action(
    world: WorldState,
    event_id: str,
    atype: str,
    params: dict,
    issues: list[ValidationIssue],
) -> None:
    prefix = f"event {event_id} action {atype}"
    if atype == "move_npc":
        if (
            params.get("npc_id") not in world.npcs
            or params.get("location_id") not in world.locations
        ):
            issues.append(ValidationIssue(f"{prefix}: bad npc/location"))
    elif atype == "set_flag":
        flag_id = params.get("flag_id")
        if flag_id not in world.flag_meta and flag_id not in world.flags:
            issues.append(ValidationIssue(f"{prefix}: unknown flag {flag_id}"))
    elif atype == "spawn_item":
        if (
            params.get("item_id") not in world.items
            or params.get("location_id") not in world.locations
        ):
            issues.append(ValidationIssue(f"{prefix}: bad item/location"))
    elif atype in {"narrate", "end_game"}:
        return
    else:
        issues.append(ValidationIssue(f"{prefix}: unknown action type"))


def validate_package(package_dir: str | Path) -> list[ValidationIssue]:
    try:
        world = load_package(package_dir)
    except (CartridgeError, Exception) as exc:  # noqa: BLE001 - surface load errors
        return [ValidationIssue(str(exc))]
    return validate_world(world)
