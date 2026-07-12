from chronicle.game.tiny import TinyRoomGame
from chronicle.providers.stub import StubProvider
from chronicle.runtime import ConsoleRuntime
from chronicle.types import GamePhase


def test_tiny_room_loop():
    game = TinyRoomGame()
    runtime = ConsoleRuntime(game, provider=StubProvider(reply="Hi."))
    events = runtime.handle_line("talk stranger")
    assert game.phase == GamePhase.IN_CONVERSATION
    events = runtime.handle_line("hello")
    assert any("Stranger:" in e.text for e in events)
    events = runtime.handle_line("bye")
    assert game.phase == GamePhase.PLAYING
