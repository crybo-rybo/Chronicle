"""Terminal renderer — keep it dumb."""

from __future__ import annotations

from chronicle.types import GameEvent


class TerminalRenderer:
    def print_events(self, events: list[GameEvent]) -> None:
        for event in events:
            if event.text:
                print(event.text)
                if event.kind in {"look", "ending", "title"}:
                    print()

    def prompt(self, prefix: str = "> ") -> str:
        try:
            return input(prefix)
        except EOFError:
            return "quit"
