"""Shared helpers for live Ollama integration tests."""

from __future__ import annotations

import os
from functools import lru_cache

import httpx
import pytest

from chronicle.providers.openai_compat import OpenAICompatProvider

DEFAULT_OLLAMA_BASE = "http://localhost:11434/v1"

# Preference order for this project's tool-calling playthroughs on ~18GB Apple Silicon.
# ministral-3:3b: proven tool calls, light KV cache.
# qwen3:8b: stronger but heavier; still fine on 18GB if already resident.
# qwen3-vl:2b intentionally omitted — vision-tuned, weaker text tool use.
PREFERRED_MODELS = (
    "ministral-3:3b",
    "qwen3:8b",
)


@lru_cache(maxsize=1)
def ollama_status() -> tuple[bool, str | None, list[str]]:
    """Return (reachable, chosen_model, available_names)."""
    base = os.environ.get("CHRONICLE_BASE_URL", DEFAULT_OLLAMA_BASE).rstrip("/")
    tags_url = base.removesuffix("/v1") + "/api/tags"
    try:
        response = httpx.get(tags_url, timeout=2.0)
        response.raise_for_status()
        names = [m.get("name", "") for m in response.json().get("models", [])]
    except (httpx.HTTPError, ValueError, KeyError):
        return False, None, []

    forced = os.environ.get("CHRONICLE_MODEL", "").strip()
    if forced:
        if forced in names or any(n.startswith(forced.split(":")[0]) for n in names):
            return True, forced, names
        return True, forced, names  # let the provider fail clearly if missing

    for preferred in PREFERRED_MODELS:
        if preferred in names:
            return True, preferred, names
    return True, (names[0] if names else None), names


def require_ollama() -> tuple[str, str]:
    reachable, model, names = ollama_status()
    if not reachable:
        pytest.skip("Ollama is not reachable on localhost:11434")
    if not model:
        pytest.skip(f"No usable Ollama models installed (have: {names})")
    base = os.environ.get("CHRONICLE_BASE_URL", DEFAULT_OLLAMA_BASE)
    return base, model


def make_ollama_provider(
    *,
    temperature: float = 0.2,
    max_tokens: int = 384,
) -> OpenAICompatProvider:
    base, model = require_ollama()
    return OpenAICompatProvider(
        base_url=base,
        model=model,
        api_key=os.environ.get("CHRONICLE_API_KEY", "not-needed"),
        timeout_s=180.0,
        temperature=temperature,
        max_tokens=max_tokens,
    )


def event_text(events) -> str:
    return "\n".join(e.text for e in events if e.text)
