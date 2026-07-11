"""OpenAI-compatible HTTP chat provider (Ollama, LM Studio, vLLM, etc.)."""

from __future__ import annotations

import uuid

import httpx

from chronicle.providers.stub import parse_tool_arguments, tool_schemas_to_openai
from chronicle.types import ChatResult, Message, MessageRole, ToolCall, ToolSpec


class ProviderError(RuntimeError):
    """Raised when the remote model endpoint fails."""


class OpenAICompatProvider:
    def __init__(
        self,
        *,
        base_url: str,
        model: str,
        api_key: str = "not-needed",
        timeout_s: float = 120.0,
        temperature: float = 0.7,
        max_tokens: int = 512,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.api_key = api_key
        self.timeout_s = timeout_s
        self.temperature = temperature
        self.max_tokens = max_tokens

    def chat(self, messages: list[Message], tools: list[ToolSpec]) -> ChatResult:
        payload: dict = {
            "model": self.model,
            "messages": [_message_to_openai(m) for m in messages],
            "temperature": self.temperature,
            "max_tokens": self.max_tokens,
        }
        if tools:
            payload["tools"] = tool_schemas_to_openai(tools)
            payload["tool_choice"] = "auto"

        url = f"{self.base_url}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }
        try:
            with httpx.Client(timeout=self.timeout_s) as client:
                response = client.post(url, json=payload, headers=headers)
                response.raise_for_status()
                data = response.json()
        except httpx.HTTPError as exc:
            raise ProviderError(f"LLM request failed: {exc}") from exc

        try:
            choice = data["choices"][0]["message"]
        except (KeyError, IndexError, TypeError) as exc:
            raise ProviderError(f"Unexpected LLM response shape: {data!r}") from exc

        content = choice.get("content") or ""
        tool_calls: list[ToolCall] = []
        for raw in choice.get("tool_calls") or []:
            function = raw.get("function") or {}
            tool_calls.append(
                ToolCall(
                    id=raw.get("id") or f"call_{uuid.uuid4().hex[:8]}",
                    name=function.get("name") or "",
                    arguments=parse_tool_arguments(function.get("arguments") or {}),
                )
            )
        return ChatResult(content=content, tool_calls=tool_calls)


def _message_to_openai(message: Message) -> dict:
    payload: dict = {"role": str(message.role), "content": message.content}
    if message.name:
        payload["name"] = message.name
    if message.tool_call_id:
        payload["tool_call_id"] = message.tool_call_id
    if message.role == MessageRole.ASSISTANT:
        # Keep shape simple; tool_calls are not replayed from history in v1.
        pass
    return payload
