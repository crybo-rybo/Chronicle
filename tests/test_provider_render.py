import json

import httpx
import pytest

from chronicle.providers.openai_compat import OpenAICompatProvider, ProviderError
from chronicle.providers.stub import StubProvider, parse_tool_arguments
from chronicle.render import TerminalRenderer
from chronicle.types import GameEvent, Message, MessageRole, ToolSpec


def test_stub_without_say_tool_returns_content():
    provider = StubProvider(reply="fallback")
    result = provider.chat(
        [Message(role=MessageRole.USER, content="hi")],
        tools=[],
    )
    assert result.content == "fallback"


def test_parse_tool_arguments_variants():
    assert parse_tool_arguments({"a": 1}) == {"a": 1}
    assert parse_tool_arguments('{"a": 1}') == {"a": 1}
    assert parse_tool_arguments("not-json") == {"text": "not-json"}
    assert parse_tool_arguments("") == {}


def test_openai_compat_success(monkeypatch):
    payload = {
        "choices": [
            {
                "message": {
                    "content": "",
                    "tool_calls": [
                        {
                            "id": "call_1",
                            "function": {
                                "name": "say",
                                "arguments": json.dumps({"text": "Hello"}),
                            },
                        }
                    ],
                }
            }
        ]
    }

    class FakeResponse:
        def raise_for_status(self):
            return None

        def json(self):
            return payload

    class FakeClient:
        def __init__(self, *args, **kwargs):
            pass

        def __enter__(self):
            return self

        def __exit__(self, *args):
            return False

        def post(self, url, json=None, headers=None):
            assert "/chat/completions" in url
            return FakeResponse()

    monkeypatch.setattr(httpx, "Client", FakeClient)
    provider = OpenAICompatProvider(base_url="http://example/v1", model="demo")
    result = provider.chat(
        [Message(role=MessageRole.USER, content="hi")],
        [ToolSpec(name="say", description="speak", parameters={"type": "object"})],
    )
    assert result.tool_calls[0].name == "say"
    assert result.tool_calls[0].arguments["text"] == "Hello"


def test_openai_compat_http_error(monkeypatch):
    class FakeClient:
        def __init__(self, *args, **kwargs):
            pass

        def __enter__(self):
            return self

        def __exit__(self, *args):
            return False

        def post(self, *args, **kwargs):
            raise httpx.ConnectError("boom")

    monkeypatch.setattr(httpx, "Client", FakeClient)
    provider = OpenAICompatProvider(base_url="http://example/v1", model="demo")
    with pytest.raises(ProviderError):
        provider.chat([Message(role=MessageRole.USER, content="hi")], [])


def test_terminal_renderer_prints(capsys):
    renderer = TerminalRenderer()
    renderer.print_events(
        [
            GameEvent(kind="look", text="A room."),
            GameEvent(kind="narration", text="Something happens."),
        ]
    )
    out = capsys.readouterr().out
    assert "A room." in out
    assert "Something happens." in out
