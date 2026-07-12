from pathlib import Path

from chronicle.game.cartridge_game import CartridgeGame
from chronicle.types import Action, ActionSource, ToolCall

ROOT = Path(__file__).resolve().parents[1]
MINIMAL = ROOT / "examples" / "minimal"
BROKEN = ROOT / "examples" / "broken_wheel"


def test_npc_say_remember_reveal_apply():
    game = CartridgeGame(MINIMAL)
    game.handle_player("talk warden")
    actions = game.interpret_tools(
        [
            ToolCall(id="1", name="say", arguments={"text": "Welcome."}),
            ToolCall(
                id="2",
                name="remember",
                arguments={"summary": "Player asked about guests", "importance": 7},
            ),
            ToolCall(id="3", name="reveal_knowledge", arguments={"fact_id": "fact_guest_arrived"}),
        ]
    )
    for action in actions:
        ok, _ = game.validate_action(action)
        assert ok, action
    events = game.apply(actions)
    assert any("Welcome" in e.text for e in events)
    assert game.world.npcs["warden"].state.memories
    assert "fact_guest_arrived" in game.world.revealed_facts
    assert any("midnight" in e.text.lower() or "guest" in e.text.lower() for e in events)


def test_marcus_give_and_mood_trust():
    game = CartridgeGame(BROKEN)
    game.handle_player("talk marcus")
    give = Action(
        type="give_item",
        source=ActionSource.NPC,
        actor_id="marcus",
        params={"item_id": "cargo_manifest"},
    )
    assert game.validate_action(give)[0]
    events = game.apply([give])
    assert "cargo_manifest" in game.world.player.inventory
    assert any("manifest" in e.text.lower() or "hands" in e.text.lower() for e in events)

    mood = Action(
        type="update_mood",
        source=ActionSource.NPC,
        actor_id="marcus",
        params={"mood": "suspicious"},
    )
    trust = Action(
        type="update_trust",
        source=ActionSource.NPC,
        actor_id="marcus",
        params={"delta": 10},
    )
    assert game.validate_action(mood)[0]
    assert game.validate_action(trust)[0]
    game.apply([mood, trust])
    assert game.world.npcs["marcus"].state.mood == "suspicious"
    assert game.world.npcs["marcus"].state.trust_toward_player == 10
