# Authoring Guide

## Quick start

1. Copy `examples/minimal`.
2. Edit `scenario.json` id/name/metadata.
3. Shape `world.json`, `npcs.json`, `facts.json`, `flags.json`, `events.json`.
4. Run `chronicle validate --scenario <dir>`.
5. Play with `chronicle --scenario <dir>` (stub dialogue, no model required).

Field-by-field notes live in [`scenario-package-schema.md`](scenario-package-schema.md).
Player commands, NPC tools, and event vocabularies live in
[`console-api.md`](console-api.md).

## Design tips

- Keep maps small and NPC counts low.
- Put mystery truth in `facts.json` and NPC `knowledge` lists — do not rely on
  the model to invent durable lore.
- Use `tool_policy` to tightly scope what each NPC may touch.
- Prefer scripted `events.json` for endings and irreversible beats.
- Model endpoints, credentials, and save locations are operator settings and
  are not cartridge fields.
- Leave `config.json` as `{}` unless you need clock, prompt-budget, or
  narration overrides. Do not commit host model URLs.

## Local models

Chronicle talks to an OpenAI-compatible `/v1/chat/completions` endpoint via
the [scry](https://github.com/crybo-rybo/scry) harness (Ollama, llama.cpp
server, and similar). It does not load GGUF files in-process.

```bash
CHRONICLE_BASE_URL=http://localhost:11434/v1 \
CHRONICLE_MODEL=qwen3:8b \
CHRONICLE_DISABLE_REASONING=1 \
chronicle --scenario examples/broken_wheel
```

`CHRONICLE_DISABLE_REASONING=1` is recommended for qwen3-class models so they
do not spend the turn budget on hidden thinking.
