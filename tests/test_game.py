from pathlib import Path

from chronicle.game.cartridge_game import CartridgeGame
from chronicle.gate import ActionGate
from chronicle.providers.stub import StubProvider
from chronicle.runtime import ConsoleRuntime
from chronicle.types import GamePhase

ROOT = Path(__file__).resolve().parents[1]
MINIMAL = ROOT / "examples" / "minimal"


def test_look_and_move():
    game = CartridgeGame(MINIMAL)
    events = game.bootstrap()
    assert any("Foyer" in e.text for e in events)
    events = game.handle_player("go east")
    assert game.world.player.current_location == "study"
    assert any("Study" in e.text for e in events)


def test_take_triggers_ending():
    game = CartridgeGame(MINIMAL)
    game.handle_player("take visitor_ledger")
    assert "visitor_ledger" in game.world.player.inventory
    game.handle_player("go east")
    events = game.after_turn()
    # move was significant; after_turn advances and evaluates events
    # Need another after_turn after go - handle via runtime style
    assert game.world.player.current_location == "study"
    # go set significant; after_turn should fire ending when ledger + study
    assert game.phase == GamePhase.GAME_OVER or any(e.kind == "ending" for e in events)


def test_runtime_talk_stub():
    game = CartridgeGame(MINIMAL)
    runtime = ConsoleRuntime(game, provider=StubProvider(reply="Hello there."))
    runtime.handle_line("talk warden")
    events = runtime.handle_line("Who are you?")
    assert any("Warden:" in e.text or "Hello there" in e.text for e in events)


def test_tool_policy_rejects_disallowed():
    game = CartridgeGame(MINIMAL)
    game.handle_player("talk warden")
    from chronicle.types import Action, ActionSource

    action = Action(
        type="set_flag",
        source=ActionSource.NPC,
        actor_id="warden",
        params={"flag_id": "nope", "value": True},
    )
    ok, reason = game.validate_action(action)
    assert not ok
    gate = ActionGate(game)
    events = gate.submit([action])
    assert any(e.kind == "warning" for e in events)


def test_save_load_roundtrip(tmp_path):
    game = CartridgeGame(MINIMAL, save_dir=tmp_path)
    game.handle_player("take visitor_ledger")
    game.save(1)
    game2 = CartridgeGame(MINIMAL, save_dir=tmp_path)
    game2.load(1)
    assert "visitor_ledger" in game2.world.player.inventory
