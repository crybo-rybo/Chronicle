"""Live Ollama playthroughs of bundled cartridge games."""

from __future__ import annotations

from pathlib import Path

import pytest
from ollama_helpers import event_text, make_ollama_provider, ollama_status, require_ollama

from chronicle.game.cartridge_game import CartridgeGame
from chronicle.runtime import ConsoleRuntime
from chronicle.types import GamePhase

ROOT = Path(__file__).resolve().parents[1]
MINIMAL = ROOT / "examples" / "minimal"
BROKEN_WHEEL = ROOT / "examples" / "broken_wheel"

pytestmark = pytest.mark.integration


@pytest.fixture(scope="module")
def ollama_provider():
    return make_ollama_provider()


@pytest.fixture(scope="module")
def selected_model() -> str:
    _, model = require_ollama()
    assert model
    return model


def test_ollama_model_selection_is_sane(selected_model: str):
    reachable, model, names = ollama_status()
    assert reachable
    assert model == selected_model
    assert names, "expected at least one local Ollama model"
    # On this machine, prefer the light tool-capable model when env is unset.
    import os

    if not os.environ.get("CHRONICLE_MODEL") and "ministral-3:3b" in names:
        assert selected_model == "ministral-3:3b"


def test_play_minimal_talk_to_warden(ollama_provider, selected_model: str):
    """Player talks to the Warden; model should use tools / produce dialogue."""
    game = CartridgeGame(MINIMAL)
    runtime = ConsoleRuntime(game, provider=ollama_provider)

    bootstrap = game.bootstrap()
    assert any("Foyer" in e.text for e in bootstrap)

    start = runtime.handle_line("talk warden")
    assert game.phase == GamePhase.IN_CONVERSATION
    assert game.active_npc_id == "warden"
    assert any("Warden" in e.text for e in start)

    reply = runtime.handle_line("Who keeps the visitor records here?")
    blob = event_text(reply)
    assert "Inference failed" not in blob, blob
    # Accept either tool-backed dialogue or plain-text fallback.
    assert "Warden:" in blob or "warden:" in blob.lower(), (
        f"model={selected_model} produced no Warden dialogue:\n{blob}"
    )
    assert len(blob.strip()) > 10


def test_play_minimal_reveal_knowledge(ollama_provider, selected_model: str):
    """Ask about the guest; model may reveal authored knowledge via tool."""
    game = CartridgeGame(MINIMAL)
    runtime = ConsoleRuntime(game, provider=ollama_provider)
    runtime.handle_line("talk warden")

    reply = runtime.handle_line(
        "Please use reveal_knowledge with fact_id exactly equal to "
        "'fact_guest_arrived', then say a short confirmation."
    )
    blob = event_text(reply)
    assert "Inference failed" not in blob, blob

    spoken = "Warden:" in blob
    revealed = "fact_guest_arrived" in game.world.revealed_facts or (
        "guest signed the ledger" in blob.lower()
    )
    remembered = bool(game.world.npcs["warden"].state.memories)
    attempted_reveal = "fact" in blob.lower() and (
        "Rejected" in blob or "not know" in blob.lower() or "not allowed" in blob.lower()
    )
    assert spoken or revealed or remembered or attempted_reveal, (
        f"model={selected_model} did not speak, reveal, remember, or attempt reveal:\n{blob}"
    )


def test_play_minimal_full_session_with_npc_then_ending(ollama_provider, selected_model: str):
    """Realistic short session: greet NPC, explore, complete scripted ending."""
    game = CartridgeGame(MINIMAL)
    runtime = ConsoleRuntime(game, provider=ollama_provider)

    transcript: list[str] = []

    def step(cmd: str) -> str:
        events = runtime.handle_line(cmd)
        text = event_text(events)
        transcript.append(f"> {cmd}\n{text}")
        return text

    step("look")
    step("talk warden")
    dialogue = step("Good evening. What is this place?")
    assert "Inference failed" not in dialogue, "\n\n".join(transcript)
    assert "Warden:" in dialogue or len(dialogue) > 0

    step("bye")
    assert game.phase == GamePhase.PLAYING

    take = step("take visitor ledger")
    assert "take" in take.lower() or "Visitor Ledger" in take
    assert "visitor_ledger" in game.world.player.inventory

    ending = step("go east")
    assert game.world.player.current_location == "study"
    assert game.phase == GamePhase.GAME_OVER or "first lead" in ending.lower(), (
        f"model={selected_model} session did not reach ending:\n" + "\n\n".join(transcript)
    )


def test_play_broken_wheel_talk_to_marcus(ollama_provider, selected_model: str):
    """Smoke the richer sample cartridge with a live NPC turn."""
    game = CartridgeGame(BROKEN_WHEEL)
    runtime = ConsoleRuntime(game, provider=ollama_provider)

    runtime.handle_line("look")
    runtime.handle_line("talk marcus")
    assert game.active_npc_id == "marcus"

    reply = runtime.handle_line("I heard cargo went missing from the docks. What can you tell me?")
    blob = event_text(reply)
    assert "Inference failed" not in blob, blob
    assert "Marcus:" in blob or len(blob.strip()) > 10, (
        f"model={selected_model} produced empty Marcus reply:\n{blob}"
    )
