# Chronicle

Chronicle is an offline console for LLM-driven NPC mystery and social-sim text
adventures. Creators author JSON **cartridges**; Chronicle supplies the runtime,
local OpenAI-compatible LLM integration, validation, prompt assembly, save/load,
and a strict tool/action gate.

The stable public contract is the CLI plus the JSON cartridge schema — not
Python internals.

## Install

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"
```

Requires Python 3.12+.

## Run

```bash
chronicle --scenario examples/minimal
chronicle validate --scenario examples/minimal
chronicle inspect --scenario examples/minimal
```

Without a model endpoint, NPC dialogue uses a deterministic stub so mechanics
remain playable. Point Chronicle at a local OpenAI-compatible server:

```bash
CHRONICLE_BASE_URL=http://localhost:11434/v1 CHRONICLE_MODEL=llama3.2 \
  chronicle --scenario examples/minimal
```

Harness smoke demo (no cartridge):

```bash
chronicle --tiny
```

## Cartridges

A cartridge is a directory with `scenario.json` plus config, world, NPCs, facts,
flags, and events. See `examples/minimal` and `examples/broken_wheel`.

Docs:

- [`docs/invariants.md`](docs/invariants.md) — non-negotiable runtime rules
- [`docs/console-api.md`](docs/console-api.md) — player commands, NPC tools, events
- [`docs/scenario-package-schema.md`](docs/scenario-package-schema.md) — package fields
- [`docs/authoring-guide.md`](docs/authoring-guide.md) — how to write a cartridge

## Library commands

```bash
chronicle install examples/minimal
chronicle list
chronicle run minimal
chronicle pack --scenario examples/minimal --output /tmp/minimal.chronicle
```

## Testing

```bash
./scripts/ci.sh            # full local CI (lint + coverage + validate)
./scripts/test.sh          # unit tests only
./scripts/integration.sh   # live Ollama playthroughs
```

`scripts/ci.sh` is the same checklist GitHub Actions runs. Integration tests are
optional and pick a local model automatically (prefer `ministral-3:3b`, then
`qwen3:8b` on ~18GB Apple Silicon). Override with:

```bash
CHRONICLE_MODEL=qwen3:8b ./scripts/integration.sh
```

## Design in one line

LLM proposes; console decides. All world changes flow through a single validated
action gate. Cartridges are data, not trusted code.
