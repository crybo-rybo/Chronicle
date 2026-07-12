"""Minimal in-process game used to prove the harness loop."""

from __future__ import annotations

from chronicle.types import (
    Action,
    ActionSource,
    GameEvent,
    GamePhase,
    Message,
    MessageRole,
    ToolCall,
    ToolSpec,
    TurnRequest,
)


class TinyRoomGame:
    """One room, one NPC, talk + say tool only."""

    def __init__(self) -> None:
        self._phase = GamePhase.PLAYING
        self._active_npc: str | None = None
        self._said: list[str] = []

    @property
    def phase(self) -> GamePhase:
        return self._phase

    @property
    def active_npc_id(self) -> str | None:
        return self._active_npc

    def bootstrap(self) -> list[GameEvent]:
        return [
            GameEvent(
                kind="look",
                text=(
                    "You stand in a bare room. A quiet stranger waits here.\n"
                    "Type 'talk stranger' to speak, or 'quit' to leave."
                ),
            )
        ]

    def handle_player(self, text: str) -> list[GameEvent]:
        raw = text.strip()
        lower = raw.lower()
        if self._phase == GamePhase.GAME_OVER:
            return [GameEvent(text="The session is over.")]

        if self._phase == GamePhase.IN_CONVERSATION:
            if lower in {"bye", "goodbye", "leave", "exit conversation"}:
                self._phase = GamePhase.PLAYING
                self._active_npc = None
                return [GameEvent(text="You end the conversation.")]
            if lower in {"look", "inventory", "help", "quit", "save", "load"}:
                return self._playing_command(lower, raw)
            return []

        return self._playing_command(lower, raw)

    def wants_llm_turn(self, text: str) -> bool:
        return self._phase == GamePhase.IN_CONVERSATION and text.strip().lower() not in {
            "bye",
            "goodbye",
            "leave",
            "exit conversation",
            "look",
            "inventory",
            "help",
            "quit",
        }

    def build_turn(self, player_text: str) -> TurnRequest:
        return TurnRequest(
            messages=[
                Message(
                    role=MessageRole.SYSTEM,
                    content="You are a quiet stranger in a bare room. Use the say tool.",
                ),
                Message(role=MessageRole.USER, content=player_text),
            ],
            tools=[
                ToolSpec(
                    name="say",
                    description="Speak aloud to the player.",
                    parameters={
                        "type": "object",
                        "properties": {"text": {"type": "string"}},
                        "required": ["text"],
                    },
                )
            ],
        )

    def interpret_tools(self, calls: list[ToolCall]) -> list[Action]:
        actions: list[Action] = []
        for call in calls:
            if call.name == "say":
                actions.append(
                    Action(
                        type="say",
                        source=ActionSource.NPC,
                        actor_id=self._active_npc or "stranger",
                        params={"text": str(call.arguments.get("text", ""))},
                    )
                )
        return actions

    def validate_action(self, action: Action) -> tuple[bool, str]:
        if action.type == "say":
            return True, ""
        return False, f"Unknown action: {action.type}"

    def apply(self, actions: list[Action]) -> list[GameEvent]:
        events: list[GameEvent] = []
        for action in actions:
            if action.type == "say":
                text = str(action.params.get("text", "")).strip()
                self._said.append(text)
                events.append(GameEvent(kind="dialogue", text=f'Stranger: "{text}"'))
        return events

    def after_turn(self) -> list[GameEvent]:
        return []

    def save(self, slot: int) -> None:
        raise NotImplementedError("TinyRoomGame does not support save")

    def load(self, slot: int) -> None:
        raise NotImplementedError("TinyRoomGame does not support load")

    def help_text(self) -> str:
        return "Commands: look, talk stranger, quit. In conversation: free text, bye."

    def _playing_command(self, lower: str, raw: str) -> list[GameEvent]:
        if lower in {"quit", "q"}:
            self._phase = GamePhase.GAME_OVER
            return [GameEvent(kind="system", text="Goodbye.")]
        if lower == "help":
            return [GameEvent(text=self.help_text())]
        if lower == "look":
            return self.bootstrap()
        if lower.startswith("talk"):
            self._phase = GamePhase.IN_CONVERSATION
            self._active_npc = "stranger"
            return [GameEvent(text="You approach the stranger.")]
        if lower == "inventory":
            return [GameEvent(text="You are carrying nothing.")]
        return [GameEvent(text=f"Unknown command: {raw}")]
