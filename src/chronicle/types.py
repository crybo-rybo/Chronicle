"""Shared runtime types."""

from __future__ import annotations

from enum import StrEnum
from typing import Any

from pydantic import BaseModel, Field


class GamePhase(StrEnum):
    PLAYING = "playing"
    IN_CONVERSATION = "in_conversation"
    GAME_OVER = "game_over"


class MessageRole(StrEnum):
    SYSTEM = "system"
    USER = "user"
    ASSISTANT = "assistant"
    TOOL = "tool"


class Message(BaseModel):
    role: MessageRole
    content: str
    name: str | None = None
    tool_call_id: str | None = None


class ToolSpec(BaseModel):
    name: str
    description: str
    parameters: dict[str, Any] = Field(default_factory=dict)


class ToolCall(BaseModel):
    id: str
    name: str
    arguments: dict[str, Any] = Field(default_factory=dict)


class ChatResult(BaseModel):
    content: str = ""
    tool_calls: list[ToolCall] = Field(default_factory=list)


class ActionSource(StrEnum):
    PLAYER = "player"
    NPC = "npc"
    SYSTEM = "system"
    EVENT = "event"


class Action(BaseModel):
    """Validated unit of world change (or display-only effect)."""

    type: str
    source: ActionSource = ActionSource.SYSTEM
    actor_id: str = ""
    params: dict[str, Any] = Field(default_factory=dict)


class GameEvent(BaseModel):
    """Something the renderer should show the player."""

    kind: str = "narration"
    text: str
    meta: dict[str, Any] = Field(default_factory=dict)


class TurnRequest(BaseModel):
    messages: list[Message]
    tools: list[ToolSpec] = Field(default_factory=list)
