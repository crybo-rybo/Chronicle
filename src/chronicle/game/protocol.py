"""Game backend protocol."""

from __future__ import annotations

from typing import Protocol, runtime_checkable

from chronicle.types import Action, GameEvent, GamePhase, ToolCall, TurnRequest


@runtime_checkable
class GameBackend(Protocol):
    @property
    def phase(self) -> GamePhase: ...

    @property
    def active_npc_id(self) -> str | None: ...

    def bootstrap(self) -> list[GameEvent]:
        """Start a new session; return intro events."""

    def handle_player(self, text: str) -> list[GameEvent]:
        """Handle a player command or conversation line (non-LLM path)."""

    def wants_llm_turn(self, text: str) -> bool:
        """True when the runtime should call the provider for this input."""

    def build_turn(self, player_text: str) -> TurnRequest:
        """Build messages + tools for the active NPC conversation turn."""

    def interpret_tools(self, calls: list[ToolCall]) -> list[Action]:
        """Map raw tool calls into actions (may still be rejected by validate)."""

    def validate_action(self, action: Action) -> tuple[bool, str]:
        """Return (ok, reason). reason is empty when ok."""

    def apply(self, actions: list[Action]) -> list[GameEvent]:
        """Apply already-validated actions and return narration events."""

    def after_turn(self) -> list[GameEvent]:
        """Clock advance + scripted events after a significant turn."""

    def save(self, slot: int) -> None: ...

    def load(self, slot: int) -> None: ...

    def help_text(self) -> str: ...
