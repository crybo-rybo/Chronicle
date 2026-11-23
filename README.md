# Chronicle

Chronicle is an offline console for LLM-driven NPC mystery and social-sim text
adventures, written in C++26. Creators author JSON **cartridges**; Chronicle
supplies the runtime, local LLM integration via
[scry](https://github.com/crybo-rybo/scry), validation, prompt assembly,
save/load, and a strict tool/action gate. NPC tool schemas are derived from
plain C++ structs at compile time with C++26 reflection (P2996), through scry's
[Scrydantic](https://github.com/crybo-rybo/scrydantic) library.

The stable public contract is the CLI plus the JSON cartridge schema — not C++
internals.

## Requirements

- **GCC 16.2+** (scry and Scrydantic need `-std=c++26 -freflection`, and earlier
  GCC 16 releases have reflection bugs)
- CMake ≥ 3.30, libcurl development headers
- Linux or macOS (macOS via Homebrew GCC 16)

Dependencies (scry v0.8.0 with the Scrydantic commit it pins, nlohmann/json,
miniz, GoogleTest) are fetched at pinned commits or content hashes by CMake.

## Build

```bash
cmake --preset dev
cmake --build --preset dev -j
```

Or, with [`just`](https://github.com/casey/just): `just build`. The presets use
the default compiler; if that is not GCC 16.2+, name one the first time you
configure, for example `CC=gcc-16 CXX=g++-16 cmake --preset dev`.

Install a release build (the executable, bundled examples, and documentation):

```bash
cmake --preset release
cmake --build --preset release -j
cmake --install build-release --prefix /your/prefix
```

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

Saves go to `~/.chronicle/saves`; `--save-dir` or `CHRONICLE_SAVE_DIR` names
another directory.

Harness smoke demo (no cartridge):

```bash
./build/src/chronicle --tiny
```

## Cartridges

A cartridge is a directory with `scenario.json` plus config, world, NPCs,
facts, flags, and events. See `examples/minimal` and `examples/broken_wheel`.
[`schemas/`](schemas/) holds JSON Schema for the manifest and each data file.
Those files are editor aids; `chronicle validate` is the authoritative check.

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

## Development

```bash
just ci            # what a pull request must pass: format, -Werror build, tests, examples
just test          # unit tests only (no model required)
just format        # clang-format the sources
just integration   # live Ollama playthrough tests (optional)
```

Without `just`: `cmake --preset ci && cmake --build --preset ci -j && ctest --preset ci`.
GitHub Actions runs the same suite under ASan and UBSan and smoke-tests an
installation.

Every change to the mechanics lands with unit tests in `tests/`. What happens
between Chronicle and a model is tested there too: `tests/test_npc_sessions.cpp`
scripts the model with scry's loopback `ScriptedServer`.

The live tests find a local Ollama server and prefer `qwen3:8b`, then `qwen3.5:9b`,
then `lfm2.5:8b`; `CHRONICLE_MODEL` names another.

## Design in one line

LLM proposes; console decides. Every runtime world change flows through a single
validated action gate. Cartridges are data, not trusted code.
