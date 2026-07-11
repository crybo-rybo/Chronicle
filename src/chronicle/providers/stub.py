"""Deterministic no-network provider for offline play and tests."""

from __future__ import annotations

import json
import uuid

from chronicle.types import ChatResult, Message, ToolCall, ToolSpec


class StubProvider:
    """Returns a simple say tool-call so cartridges remain playable without a model."""

    def __init__(self, reply: str = "The figure regards you quietly.") -> None:
        self.reply = reply

    def chat(self, messages: list[Message], tools: list[ToolSpec]) -> ChatResult:
        tool_names = {t.name for t in tools}
        if "say" in tool_names:
            return ChatResult(
                content="",
                tool_calls=[
                    ToolCall(
                        id=f"stub_{uuid.uuid4().hex[:8]}",
                        name="say",
                        arguments={"text": self.reply},
                    )
                ],
            )
        # Fall back to plain text if say is unavailable.
        last_user = next((m.content for m in reversed(messages) if m.role == "user"), "")
        return ChatResult(content=self.reply or last_user or "...")


def tool_schemas_to_openai(tools: list[ToolSpec]) -> list[dict]:
    return [
        {
            "type": "function",
            "function": {
                "name": t.name,
                "description": t.description,
                "parameters": t.parameters
                or {"type": "object", "properties": {}, "additionalProperties": False},
            },
        }
        for t in tools
    ]


def parse_tool_arguments(raw: str | dict) -> dict:
    if isinstance(raw, dict):
        return raw
    if not raw:
        return {}
    try:
        value = json.loads(raw)
    except json.JSONDecodeError:
        return {"text": raw}
    return value if isinstance(value, dict) else {"value": value}
