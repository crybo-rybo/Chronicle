"""Live LLM provider smoke tests against local Ollama."""

from __future__ import annotations

from ollama_helpers import make_ollama_provider, require_ollama

from chronicle.types import Message, MessageRole, ToolSpec

pytestmark = __import__("pytest").mark.integration


def test_live_openai_compat_roundtrip():
    provider = make_ollama_provider()
    _, model = require_ollama()
    result = provider.chat(
        [Message(role=MessageRole.USER, content="Reply with the single word: pong")],
        tools=[],
    )
    assert result.content.strip(), f"empty completion from {model}"


def test_live_tool_call_say():
    provider = make_ollama_provider(temperature=0.1)
    _, model = require_ollama()
    result = provider.chat(
        [
            Message(
                role=MessageRole.SYSTEM,
                content="You are a caretaker. Always use the say tool to speak.",
            ),
            Message(role=MessageRole.USER, content="Who are you?"),
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
    assert result.tool_calls or result.content.strip(), f"no usable output from {model}"
    if result.tool_calls:
        assert result.tool_calls[0].name == "say"
        assert str(result.tool_calls[0].arguments.get("text", "")).strip()
