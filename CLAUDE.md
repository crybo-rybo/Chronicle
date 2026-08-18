# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Chronicle is an offline console for LLM-driven NPC mystery / social-sim text
adventures, written in **C++26**. Creators author JSON **cartridges** (scenario
packages); Chronicle supplies the runtime, validation, prompt assembly,
save/load, and a strict tool/action gate. LLM integration is the
[scry](https://github.com/crybo-rybo/scry) harness; NPC tool schemas are
generated at compile time from plain structs via C++26 reflection (P2996).

The stable public contract is the CLI plus the JSON cartridge schema —
internal C++ may change freely. `examples/minimal` and `examples/broken_wheel`
must always validate and play.

## Toolchain and commands

**GCC 16+ only** — the reflection component needs `-std=c++26 -freflection`;
Clang cannot build this project (`.clangd` strips the flag so the LSP can
still parse). Dependencies (scry v0.1.0, nlohmann/json, miniz, GTest) are
fetched by CMake at pinned commits or content hashes; libcurl dev headers must
be installed.

- `just ci` — formatting + warning-clean build + tests + example and install smoke
- `just test` — unit tests only (`cmake --preset dev && ctest --preset dev`)
- `just integration` — live Ollama playthroughs (`ctest --preset integration`);
  auto-detects a local model, prefers `qwen3:8b`; override with `CHRONICLE_MODEL`
- Single test: `./build/tests/chronicle_tests --gtest_filter='GateTest.*'`
- Run the game: `./build/src/chronicle --scenario examples/minimal` (stub
  dialogue without an endpoint; set `CHRONICLE_BASE_URL`/`CHRONICLE_MODEL` for
  a live model, `CHRONICLE_DISABLE_REASONING=1` for qwen3-class models)

CI uses `-DCHRONICLE_WERROR=ON`; keep the build warning-clean, including in
tests (watch `-Wmissing-field-initializers` on partial designated init of
structs whose members lack default initializers).

## Architecture

Core invariant: **LLM proposes; console decides** (`docs/invariants.md` — read
before touching the runtime). Every world change is a typed action submitted
through `CartridgeGame::submit_world_action`. Player commands, model tools,
scripted events, clock changes, and persistence restores all use that one
boundary. Model calls first pass through `submit_npc_tool` for per-NPC
authorization, then translate into the same world actions. A rejection is
returned to the model as structured data so it can react — never a crash or a
silent write.

Three layers (`src/CMakeLists.txt`):

- **`chronicle_core`** (no scry, plain C++): `cartridge/` (models + JSON via
  nlohmann, path-confined loader, cross-reference validator), `game/`
  (`cartridge_game.cpp` — session lifecycle and player commands;
  `npc_tool_gate.cpp` — model authorization and translation;
  `scripted_events.cpp` — event evaluation;
  `npc_tools.hpp` — the ten tool argument structs), `persist`, `library`
  (miniz zip pack/install under `~/.chronicle/cartridges`), `prompt`, `render`.
- **`chronicle_llm`** (compiled with `-freflection`): `llm/npc_sessions.cpp`
  — one scry `Harness` + persistent `Conversation` per talked-to NPC, created
  lazily; registers exactly the NPC's allowed tools via
  `scry::reflection::add<Args>` with handlers that call the gate. `runtime.cpp`
  drives the blocking turn loop (`send_and_wait`); `cli.cpp` parses args and
  owns subcommands (`run/validate/inspect/list/install/pack`, `--tiny`).
- Tests: `tests/*.cpp` unit (150+, no network — gate, mechanics, events,
  persistence, library, prompts, CLI, stub runtime), `tests/integration/`
  live-Ollama playthroughs that skip cleanly when no server is up.

Key couplings to keep in sync:

- The reflected adapter aggregates and compile-time binding table in
  `llm/npc_sessions.cpp` are the schema source of truth; they convert into the
  named domain calls in `game/npc_tools.hpp`, which feed the authorization
  gate. A compile-time assertion keeps both catalogs exact. Adding a tool
  touches those files, the gate visitors in `npc_tool_gate.cpp`,
  `docs/console-api.md`, and `schemas/scenario.schema.json`.
- The static NPC system prompt (identity/knowledge/rules) is fixed at
  `Conversation` creation; **dynamic state (time, mood, trust, secret,
  memories) must go in the per-turn user message** (`prompt.cpp`), never the
  system prompt — conversations persist across turns and into save files.
- No endpoint → no Harness is ever created; the stub path routes a canned
  `say` through the same gate (invariant 5: mechanics work with no model).
- A model turn is transactional across Scry history and the world. Provider or
  harness failure restores the complete runtime checkpoint before deterministic
  stub dialogue is shown.
