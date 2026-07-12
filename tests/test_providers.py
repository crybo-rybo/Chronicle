from chronicle.providers.stub import StubProvider
from chronicle.types import Message, MessageRole, ToolSpec


def test_stub_uses_say_tool():
    provider = StubProvider(reply="Ping")
    result = provider.chat(
        [Message(role=MessageRole.USER, content="hi")],
        [ToolSpec(name="say", description="speak", parameters={"type": "object"})],
    )
    assert result.tool_calls
    assert result.tool_calls[0].name == "say"
    assert result.tool_calls[0].arguments["text"] == "Ping"
