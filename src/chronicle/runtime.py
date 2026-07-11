"""Console runtime: turn loop, provider calls, action gate, degrade."""

from __future__ import annotations

from chronicle.game.protocol import GameBackend
from chronicle.gate import ActionGate
from chronicle.providers import LLMProvider
from chronicle.providers.openai_compat import ProviderError
from chronicle.providers.stub import StubProvider
from chronicle.render import TerminalRenderer
from chronicle.types import GameEvent, GamePhase


class ConsoleRuntime:
    def __init__(
        self,
        game: GameBackend,
        provider: LLMProvider | None = None,
        renderer: TerminalRenderer | None = None,
    ) -> None:
        self.game = game
        self.provider: LLMProvider = provider or StubProvider()
        self.renderer = renderer or TerminalRenderer()
        self.gate = ActionGate(game)

    def run(self) -> int:
        self.renderer.print_events(self.game.bootstrap())
        while self.game.phase != GamePhase.GAME_OVER:
            line = self.renderer.prompt()
            events = self.handle_line(line)
            self.renderer.print_events(events)
        return 0

    def handle_line(self, line: str) -> list[GameEvent]:
        events: list[GameEvent] = []
        text = line.strip()
        if not text:
            return events

        # Player / hard-command path first.
        events.extend(self.game.handle_player(text))

        if self.game.phase == GamePhase.GAME_OVER:
            return events

        if self.game.wants_llm_turn(text):
            events.extend(self._run_llm_turn(text))

        # Significant turns (movement, items, successful dialogue tools) advance clock/events.
        events.extend(self.game.after_turn())
        return events

    def _run_llm_turn(self, player_text: str) -> list[GameEvent]:
        turn = self.game.build_turn(player_text)
        try:
            result = self.provider.chat(turn.messages, turn.tools)
        except ProviderError as exc:
            return [
                GameEvent(
                    kind="warning",
                    text=(
                        f"Inference failed ({exc}). "
                        "The conversation falters, but the world remains."
                    ),
                )
            ]
        except Exception as exc:  # noqa: BLE001 - degrade on any provider failure
            return [
                GameEvent(
                    kind="warning",
                    text=(
                        f"Inference failed ({exc}). "
                        "The conversation falters, but the world remains."
                    ),
                )
            ]

        events: list[GameEvent] = []
        if result.content.strip() and not result.tool_calls:
            # Plain-text fallback when the model ignores tools.
            npc = self.game.active_npc_id or "NPC"
            events.append(GameEvent(kind="dialogue", text=f'{npc}: "{result.content.strip()}"'))

        if result.tool_calls:
            actions = self.game.interpret_tools(result.tool_calls)
            events.extend(self.gate.submit(actions))
        return events
