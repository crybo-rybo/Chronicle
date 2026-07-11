"""LLM providers."""

from typing import Protocol, runtime_checkable

from chronicle.providers.openai_compat import OpenAICompatProvider, ProviderError
from chronicle.providers.stub import StubProvider
from chronicle.types import ChatResult, Message, ToolSpec


@runtime_checkable
class LLMProvider(Protocol):
    def chat(self, messages: list[Message], tools: list[ToolSpec]) -> ChatResult: ...


__all__ = [
    "LLMProvider",
    "OpenAICompatProvider",
    "ProviderError",
    "StubProvider",
]
