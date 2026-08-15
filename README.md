# Chronicle

Chronicle is an offline console for LLM-driven NPC mystery and social-sim text
adventures, written in C++26. Creators author JSON **cartridges**; Chronicle
supplies the runtime, local LLM integration via
[scry](https://github.com/crybo-rybo/scry), validation, prompt assembly,
save/load, and a strict tool/action gate. NPC tool schemas are derived from
plain C++ structs at compile time with C++26 reflection (P2996).

The stable public contract is the CLI plus the JSON cartridge schema — not C++
internals.

## Requirements

- **GCC 16+** (the reflection component needs `-std=c++26 -freflection`)
- CMake ≥ 3.25, libcurl development headers
- Linux or macOS (macOS via Homebrew GCC 16)

Dependencies (scry, nlohmann/json, miniz, GoogleTest) are fetched by CMake.

## Build

```bash
cmake --preset dev
cmake --build --preset dev -j
```

Or, with [`just`](https://github.com/casey/just): `just build`.

## Run

```bash
./build/src/chronicle --scenario examples/minimal
./build/src/chronicle validate --scenario examples/minimal
./build/src/chronicle inspect --scenario examples/minimal
```

Without a model endpoint, NPC dialogue uses a deterministic stub so mechanics
remain playable. Point Chronicle at a local OpenAI-compatible server:

```bash
CHRONICLE_BASE_URL=http://localhost:11434/v1 CHRONICLE_MODEL=qwen3:8b \
  ./build/src/chronicle --scenario examples/minimal
```

Environment variables: `CHRONICLE_BASE_URL`, `CHRONICLE_MODEL`,
`CHRONICLE_API_KEY` (or `OPENAI_API_KEY`), `CHRONICLE_DIALECT=anthropic` for
the Anthropic Messages API, and `CHRONICLE_DISABLE_REASONING=1` to request
`reasoning_effort: "none"` from endpoints that support it (recommended for
qwen3-class models).

Harness smoke demo (no cartridge):

```bash
./build/src/chronicle --tiny
```

## Cartridges

A cartridge is a directory with `scenario.json` plus config, world, NPCs,
facts, flags, and events. See `examples/minimal` and `examples/broken_wheel`.

Docs:

- [`docs/invariants.md`](docs/invariants.md) — non-negotiable runtime rules
- [`docs/console-api.md`](docs/console-api.md) — player commands, NPC tools, events
- [`docs/scenario-package-schema.md`](docs/scenario-package-schema.md) — package fields
- [`docs/authoring-guide.md`](docs/authoring-guide.md) — how to write a cartridge

## Library commands

```bash
./build/src/chronicle install examples/minimal
./build/src/chronicle list
./build/src/chronicle run minimal
./build/src/chronicle pack --scenario examples/minimal --output /tmp/minimal.chronicle
```

## Testing

```bash
just ci            # build + unit tests + validate examples (same as GitHub Actions)
just test          # unit tests only (no model required)
just integration   # live Ollama playthrough tests
```

Without `just`: `ctest --preset dev` and `ctest --preset integration`.

Integration tests are optional; they auto-detect a local Ollama server and
prefer `qwen3:8b`, then `qwen3.5:9b`, then `lfm2.5:8b`. Override with:

```bash
CHRONICLE_MODEL=qwen3.5:9b just integration
```

## Design in one line

LLM proposes; console decides. All model-driven world changes flow through a
single validated action gate. Cartridges are data, not trusted code.
