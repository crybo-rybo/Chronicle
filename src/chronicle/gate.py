"""Single action gate: validate then apply through the game backend."""

from __future__ import annotations

from chronicle.game.protocol import GameBackend
from chronicle.types import Action, GameEvent


class ActionGate:
    """All world writes flow through validate → apply."""

    def __init__(self, game: GameBackend) -> None:
        self._game = game

    def submit(self, actions: list[Action]) -> list[GameEvent]:
        accepted: list[Action] = []
        events: list[GameEvent] = []
        for action in actions:
            ok, reason = self._game.validate_action(action)
            if not ok:
                events.append(
                    GameEvent(
                        kind="warning",
                        text=reason or f"Rejected action: {action.type}",
                        meta={"action": action.type},
                    )
                )
                continue
            accepted.append(action)
        if accepted:
            events.extend(self._game.apply(accepted))
        return events
