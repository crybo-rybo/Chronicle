from pathlib import Path

from chronicle.game.cartridge_game import CartridgeGame
from chronicle.providers.stub import StubProvider
from chronicle.runtime import ConsoleRuntime
from chronicle.types import GamePhase

ROOT = Path(__file__).resolve().parents[1]
MINIMAL = ROOT / "examples" / "minimal"
BROKEN = ROOT / "examples" / "broken_wheel"


def test_examine_drop_inventory_help():
    game = CartridgeGame(MINIMAL)
    game.handle_player("take visitor_ledger")
    events = game.handle_player("examine visitor_ledger")
    assert any("ledger" in e.text.lower() or "page" in e.text.lower() for e in events)
    events = game.handle_player("inventory")
    assert any("Visitor Ledger" in e.text for e in events)
    events = game.handle_player("drop visitor_ledger")
    assert "visitor_ledger" not in game.world.player.inventory
    events = game.handle_player("help")
    assert any("go" in e.text for e in events)


def test_conversation_hard_commands_and_bye():
    game = CartridgeGame(MINIMAL)
    runtime = ConsoleRuntime(game, provider=StubProvider(reply="Noted."))
    runtime.handle_line("talk warden")
    assert game.phase == GamePhase.IN_CONVERSATION
    events = runtime.handle_line("inventory")
    assert any("carrying" in e.text.lower() or "nothing" in e.text.lower() for e in events)
    events = runtime.handle_line("bye")
    assert game.phase == GamePhase.PLAYING


def test_broken_wheel_look_and_talk_stub():
    game = CartridgeGame(BROKEN)
    runtime = ConsoleRuntime(game, provider=StubProvider(reply="Keep your voice down."))
    events = runtime.handle_line("look")
    assert any("Broken Wheel" in e.text or "Tavern" in e.text for e in events)
    runtime.handle_line("talk marcus")
    events = runtime.handle_line("What happened to the cargo?")
    assert any("Marcus:" in e.text for e in events)


def test_unknown_command():
    game = CartridgeGame(MINIMAL)
    events = game.handle_player("dance")
    assert any("Unknown" in e.text for e in events)


def test_go_invalid_direction():
    game = CartridgeGame(MINIMAL)
    events = game.handle_player("go up")
    assert any("can't go" in e.text.lower() for e in events)
