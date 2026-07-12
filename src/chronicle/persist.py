"""Versioned save/load bound to a cartridge id/version."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from chronicle.cartridge.models import WorldState

SAVE_SCHEMA_VERSION = 1


class SaveSystem:
    def __init__(self, directory: str | Path) -> None:
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)

    def path_for(self, slot: int) -> Path:
        return self.directory / f"slot_{slot}.json"

    def save(
        self,
        slot: int,
        world: WorldState,
        *,
        phase: str,
        active_npc: str | None,
    ) -> None:
        payload = {
            "save_schema_version": SAVE_SCHEMA_VERSION,
            "cartridge_id": world.manifest.id,
            "cartridge_version": world.manifest.version,
            "phase": str(phase),
            "active_npc": active_npc,
            "world": world.model_dump(mode="json"),
        }
        self.path_for(slot).write_text(json.dumps(payload, indent=2), encoding="utf-8")

    def load(self, slot: int) -> dict[str, Any]:
        path = self.path_for(slot)
        if not path.exists():
            raise FileNotFoundError(path)
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("save_schema_version") != SAVE_SCHEMA_VERSION:
            raise ValueError(f"Unsupported save schema: {data.get('save_schema_version')}")
        return data
