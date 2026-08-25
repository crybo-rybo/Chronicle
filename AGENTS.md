# Chronicle

Chronicle is a bounded offline console for LLM-driven NPC mystery and social-sim
text adventures, written in C++26. Creators author JSON scenario cartridges;
Chronicle supplies the runtime, LLM integration via the scry harness (with
C++26-reflection-derived NPC tool schemas), validation, prompt assembly,
save/load, and a strict tool/action gate.

When starting a new session, read `README.md` first, then `docs/invariants.md`,
`docs/console-api.md`, and `docs/scenario-package-schema.md`.

## Invariants

1. LLM proposes; console decides.
2. Single action gate for all world writes.
3. Deterministic durable state.
4. Authored knowledge only.
5. Graceful degradation without a model.
6. Cartridges are untrusted data.
7. Public contract = CLI + cartridge schema.

## Layout

- `src/chronicle/` — runtime (cartridge/, game/, llm/ subsystems)
- `examples/` — sample cartridges
- `docs/` — creator and design docs
- `schemas/` — JSON Schema for each cartridge file (editor aids)
- `tests/` — GoogleTest unit tests (no model) + `tests/integration/` (live Ollama)

## Toolchain

GCC 16+ only (`-std=c++26 -freflection`). Build with `cmake --preset dev`,
test with `ctest --preset dev`, or use the `justfile` recipes.

## Session cleanup

Remove any files generated under `docs/superpowers/` during the session.
