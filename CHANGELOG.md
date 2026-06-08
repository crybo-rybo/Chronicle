# Changelog

All notable changes to Chronicle are documented here.

## Unreleased

### Added

- Cartridge library commands: `chronicle list`, `chronicle run <id>`, `chronicle install`, and `chronicle pack`.
- `.chronicle` gzip-compressed tar cartridge archives for distribution.
- Cartridge-bound save slots under `saves/<scenario_id>/` with metadata validation on load.
- [`docs/console-api.md`](docs/console-api.md) catalog of player commands, NPC tools, events, and mutations.
- [`docs/schema-v2-mechanics.md`](docs/schema-v2-mechanics.md) design draft for the next schema expansion.
- OpenAI-compatible endpoint configuration through `llm_base_url`, `llm_model`, and related `llm_*` settings.

### Changed

- `facts.revealed_by_default` now seeds the player's `known_facts` at world load.
- `config.total_periods` now triggers a generic time-expired ending when the clock reaches the final period.
- Public schema documents `verb_aliases` in `config.json`.
- Replaced the previous local-model integration with zoo-keeper-harness `zoo::Agent`.
- Harness tool registration now happens at `zoo::Agent::Builder` construction time.
- Integration tests now target OpenAI-compatible endpoints and skip unless endpoint variables are set.

### Removed

- Removed GGUF-local runtime configuration fields: `model_path`, `context_size`, `n_gpu_layers`, and `auto_configure`.

## v1.0.0 - 2026-05-08

### Added

- Stable v1 public contract: CLI runner, CLI validator, and JSON scenario package schema.
- Scenario package manifest loading, path-safety checks, schema version enforcement, and validator diagnostics.
- Bundled `data/` sample package plus `examples/minimal_scenario/` and `examples/lighthouse_veil/`.
- Deterministic world state for locations, items, NPC state, facts, flags, scripted events, saves, and loads.
- Runtime command support for movement, look, examine/readable items, take/drop, locked-exit item use, NPC dialogue, inventory, save/load, help, and quit.
- Local LLM integration with prompt assembly, scoped NPC tool policy, validated tool calls, explicit `remember` memory entries, and mutation-gated state changes.
- Stub dialogue mode when no endpoint is configured, plus timeout/failure recovery for model-backed dialogue.
- Google Test coverage for engine, validator, prompt contract, persistence, tools, CLI, and rendering behavior.

### Notes

- Chronicle remains fully offline when pointed at a local OpenAI-compatible endpoint. Model-backed tests are opt-in through endpoint environment variables.
- C++ APIs are implementation details for v1; the stable author-facing surface is the CLI plus JSON package schema.
