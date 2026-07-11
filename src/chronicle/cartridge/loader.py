"""Load and path-confine scenario packages."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from chronicle.cartridge.models import (
    ClockState,
    ConfigData,
    EventsFile,
    FactsFile,
    FlagsFile,
    NpcsFile,
    PlayerState,
    ScenarioManifest,
    WorldData,
    WorldState,
)


class CartridgeError(ValueError):
    """Raised when a package cannot be loaded."""


def _read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise CartridgeError(f"Missing package file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise CartridgeError(f"Invalid JSON in {path}: {exc}") from exc


def _resolve_inside(root: Path, relative: str) -> Path:
    if not relative or relative.startswith(("/", "\\")) or ".." in Path(relative).parts:
        raise CartridgeError(f"Unsafe package path: {relative!r}")
    resolved = (root / relative).resolve()
    root_resolved = root.resolve()
    if not str(resolved).startswith(str(root_resolved)):
        raise CartridgeError(f"Path escapes package directory: {relative!r}")
    return resolved


def load_manifest(package_dir: Path) -> ScenarioManifest:
    raw = _read_json(package_dir / "scenario.json")
    return ScenarioManifest.model_validate(raw)


def load_package(package_dir: str | Path) -> WorldState:
    root = Path(package_dir).resolve()
    if not root.is_dir():
        raise CartridgeError(f"Not a directory: {root}")

    manifest = load_manifest(root)
    files = manifest.files

    config_raw = _read_json(_resolve_inside(root, files.config))
    world_raw = _read_json(_resolve_inside(root, files.world))
    npcs_raw = _read_json(_resolve_inside(root, files.npcs))
    facts_raw = _read_json(_resolve_inside(root, files.facts))
    flags_raw = _read_json(_resolve_inside(root, files.flags))
    events_raw = _read_json(_resolve_inside(root, files.events))

    config = ConfigData.model_validate(config_raw if config_raw else {})
    world = WorldData.model_validate(world_raw)
    npcs_file = NpcsFile.model_validate(npcs_raw)
    facts_file = FactsFile.model_validate(facts_raw if facts_raw else {"facts": {}})
    flags_file = FlagsFile.model_validate(flags_raw if flags_raw else {"flags": {}})
    events_file = EventsFile.model_validate(events_raw if events_raw else {"events": {}})

    # Ensure npc identity ids match map keys.
    for npc_id, npc in npcs_file.npcs.items():
        if not npc.identity.id:
            npc.identity.id = npc_id

    flag_values = {fid: meta.default for fid, meta in flags_file.flags.items()}

    item_owners: dict[str, str] = {}
    item_locations: dict[str, str] = {}

    for loc_id, loc in world.locations.items():
        for item_id in loc.items:
            item_owners[item_id] = "location"
            item_locations[item_id] = loc_id

    for npc_id, npc in npcs_file.npcs.items():
        for item_id in npc.state.inventory:
            item_owners[item_id] = npc_id
            item_locations[item_id] = npc.state.current_location

    revealed = {fid for fid, fact in facts_file.facts.items() if fact.revealed_by_default}

    return WorldState(
        manifest=manifest,
        config=config,
        locations=world.locations,
        items=world.items,
        npcs=npcs_file.npcs,
        facts=facts_file.facts,
        flags=flag_values,
        flag_meta=flags_file.flags,
        events=events_file.events,
        player=PlayerState(current_location=world.start_location, inventory=[]),
        clock=ClockState(
            turns_elapsed=0,
            turns_per_period=config.turns_per_period,
            total_periods=config.total_periods,
        ),
        revealed_facts=revealed,
        item_locations=item_locations,
        item_owners=item_owners,
    )
