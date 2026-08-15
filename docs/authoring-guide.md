# Authoring Guide

## Quick start

1. Copy `examples/minimal`.
2. Edit `scenario.json` id/name/metadata.
3. Shape `world.json`, `npcs.json`, `facts.json`, `flags.json`, `events.json`.
4. Run `chronicle validate --scenario <dir>`.
5. Play with `chronicle --scenario <dir>` (stub dialogue, no model required).

## Design tips

- Keep maps small and NPC counts low.
- Put mystery truth in `facts.json` and NPC `knowledge` lists — do not rely on the
  model to invent durable lore.
- Use `tool_policy` to tightly scope what each NPC may touch.
- Prefer scripted `events.json` for endings and irreversible beats.
- Leave `llm_*` fields empty in shared cartridges; operators supply endpoints.

## Local models

Any OpenAI-compatible `/v1/chat/completions` endpoint works:

```bash
CHRONICLE_BASE_URL=http://localhost:11434/v1 \
CHRONICLE_MODEL=llama3.2 \
chronicle --scenario examples/broken_wheel
```
