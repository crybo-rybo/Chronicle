"""Pydantic models for Chronicle cartridge packages."""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel, Field, field_validator

SCHEMA_VERSION = 1
VALID_MOODS = frozenset({"fearful", "friendly", "grieving", "hostile", "neutral", "suspicious"})
NPC_TOOLS = frozenset(
    {
        "say",
        "give_item",
        "take_item",
        "update_mood",
        "update_trust",
        "move_self",
        "reveal_knowledge",
        "remember",
        "set_flag",
        "inspect_item",
    }
)
PERIOD_NAMES = ("morning", "afternoon", "evening", "night")


class ScenarioFiles(BaseModel):
    config: str
    world: str
    npcs: str
    facts: str
    flags: str
    events: str


class ScenarioMetadata(BaseModel):
    description: str = ""
    author: str = ""
    license: str = ""


class ScenarioManifest(BaseModel):
    id: str
    name: str
    version: str
    chronicle_schema_version: int
    files: ScenarioFiles
    metadata: ScenarioMetadata = Field(default_factory=ScenarioMetadata)


class ConfigData(BaseModel):
    temperature: float = 0.7
    max_response_tokens: int = 512
    inference_timeout_ms: int = 120_000
    turns_per_period: int = 5
    total_periods: int = 12
    max_memory_tokens: int = 800
    max_world_tokens: int = 400
    max_history_tokens: int = 600
    save_directory: str = ""
    use_tui: bool = False
    use_color: bool = True
    llm_base_url: str = ""
    llm_model: str = ""
    llm_api_key: str = ""
    verb_aliases: dict[str, str] = Field(default_factory=dict)
    mutation_narration_templates: dict[str, str] = Field(
        default_factory=lambda: {
            "give_item_to_player": "{npc} hands you the {item}.",
            "take_item_from_player": "{npc} takes the {item}.",
            "update_npc_mood": "{npc}'s expression shifts - they seem {mood} now.",
            "move_npc": "{npc} excuses themselves and leaves.",
            "reveal_knowledge": "",
            "update_npc_trust": "",
            "add_memory": "",
            "set_flag": "",
        }
    )

    def has_llm_endpoint(self) -> bool:
        return bool(self.llm_base_url.strip() and self.llm_model.strip())


class LockedExit(BaseModel):
    direction: str
    unlocked: bool = False


class LocationData(BaseModel):
    name: str
    base_description: str
    exits: dict[str, str] = Field(default_factory=dict)
    items: list[str] = Field(default_factory=list)
    npcs: list[str] = Field(default_factory=list)
    locked_exits: list[Any] = Field(default_factory=list)

    def locked_directions(self) -> set[str]:
        locked: set[str] = set()
        for entry in self.locked_exits:
            if isinstance(entry, str):
                locked.add(entry)
            elif isinstance(entry, dict) and entry.get("direction"):
                if not entry.get("unlocked", False):
                    locked.add(str(entry["direction"]))
        return locked


class ItemData(BaseModel):
    name: str
    description: str
    takeable: bool = True
    key_item: bool = False
    hidden: bool = False
    unlock_target: str = ""
    properties: dict[str, str] = Field(default_factory=dict)


class WorldData(BaseModel):
    start_location: str
    locations: dict[str, LocationData]
    items: dict[str, ItemData] = Field(default_factory=dict)


class ToolPolicy(BaseModel):
    allowed_tools: list[str] = Field(default_factory=list)
    allowed_items: list[str] = Field(default_factory=list)
    allowed_facts: list[str] = Field(default_factory=list)
    allowed_flags: list[str] = Field(default_factory=list)
    allowed_locations: list[str] = Field(default_factory=list)


class NpcIdentity(BaseModel):
    id: str = ""
    name: str
    role: str = ""
    personality_summary: str = ""
    backstory: str = ""
    secret: str = ""
    goals: list[str] = Field(default_factory=list)
    knowledge: list[str] = Field(default_factory=list)
    trust_reveal_threshold: int = 70
    tool_policy: ToolPolicy = Field(default_factory=ToolPolicy)


class MemoryEntry(BaseModel):
    timestamp: str = ""
    type: str = "observation"
    summary: str
    importance: int = 5
    related_npc: str = ""
    related_item: str = ""


class NpcState(BaseModel):
    current_location: str
    mood: str = "neutral"
    trust_toward_player: int = 0
    inventory: list[str] = Field(default_factory=list)
    memories: list[MemoryEntry] = Field(default_factory=list)
    has_met_player: bool = False
    secret_revealed: bool = False

    @field_validator("mood")
    @classmethod
    def _mood(cls, value: str) -> str:
        if value not in VALID_MOODS:
            raise ValueError(f"invalid mood: {value}")
        return value


class NpcData(BaseModel):
    identity: NpcIdentity
    state: NpcState


class NpcsFile(BaseModel):
    npcs: dict[str, NpcData]


class FactData(BaseModel):
    text: str
    category: str = "clue"
    revealed_by_default: bool = False


class FactsFile(BaseModel):
    facts: dict[str, FactData] = Field(default_factory=dict)


class FlagData(BaseModel):
    default: bool = False
    description: str = ""


class FlagsFile(BaseModel):
    flags: dict[str, FlagData] = Field(default_factory=dict)


class ConditionData(BaseModel):
    type: str
    args: list[str] = Field(default_factory=list)


class EventActionData(BaseModel):
    type: str
    params: dict[str, Any] = Field(default_factory=dict)


class EventTriggerData(BaseModel):
    conditions: list[ConditionData] = Field(default_factory=list)
    actions: list[EventActionData] = Field(default_factory=list)
    once: bool = True
    fired: bool = False


class EventsFile(BaseModel):
    events: dict[str, EventTriggerData] = Field(default_factory=dict)


class PlayerState(BaseModel):
    current_location: str
    inventory: list[str] = Field(default_factory=list)


class ClockState(BaseModel):
    turns_elapsed: int = 0
    turns_per_period: int = 5
    total_periods: int = 12

    @property
    def period_index(self) -> int:
        if self.turns_per_period <= 0:
            return 0
        return self.turns_elapsed // self.turns_per_period

    @property
    def period_name(self) -> str:
        return PERIOD_NAMES[self.period_index % len(PERIOD_NAMES)]

    def time_expired(self) -> bool:
        return self.period_index >= self.total_periods


class WorldState(BaseModel):
    """Runtime mutable world assembled from a cartridge."""

    manifest: ScenarioManifest
    config: ConfigData
    locations: dict[str, LocationData]
    items: dict[str, ItemData]
    npcs: dict[str, NpcData]
    facts: dict[str, FactData]
    flags: dict[str, bool]
    flag_meta: dict[str, FlagData] = Field(default_factory=dict)
    events: dict[str, EventTriggerData]
    player: PlayerState
    clock: ClockState
    revealed_facts: set[str] = Field(default_factory=set)
    item_locations: dict[str, str] = Field(default_factory=dict)
    # item_id -> location_id | "player" | npc_id
    item_owners: dict[str, str] = Field(default_factory=dict)
