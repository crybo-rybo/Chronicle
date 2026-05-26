# Changelog

All notable changes to Chronicle are documented here.

## Unreleased

### Added

- Cartridge library commands: `chronicle list`, `chronicle run <id>`, `chronicle install`, and `chronicle pack`.
- `.chronicle` gzip-compressed tar cartridge archives for distribution.
- Cartridge-bound save slots under `saves/<scenario_id>/` with metadata validation on load.
- [`docs/console-api.md`](docs/console-api.md) catalog of player commands, NPC tools, events, and mutations.
- [`docs/schema-v2-mechanics.md`](docs/schema-v2-mechanics.md) design draft for the next schema expansion.

### Changed

- `facts.revealed_by_default` now seeds the player's `known_facts` at world load.
- `config.total_periods` now triggers a generic time-expired ending when the clock reaches the final period.
- Public schema documents `verb_aliases` in `config.json`.
- Bumped Zoo-Keeper dependency from v1.1.4 to v1.1.6 (llama.cpp b9296).
- Agent command calls now use Zoo-Keeper `try_*` / `Expected<T>` APIs instead of removed void helpers.
- Optional `auto_configure` in `config.json` enables hardware-aware model loading via `zoo::load_model_config()`.
- Diagnostic logging enables Zoo-Keeper per-call tool trace capture when Chronicle logging is on.

## v1.0.0 - 2026-05-08

### Added

- Stable v1 public contract: CLI runner, CLI validator, and JSON scenario package schema.
- Scenario package manifest loading, path-safety checks, schema version enforcement, and validator diagnostics.
- Bundled `data/` sample package plus `examples/minimal_scenario/` and `examples/lighthouse_veil/`.
- Deterministic world state for locations, items, NPC state, facts, flags, scripted events, saves, and loads.
- Runtime command support for movement, look, examine/readable items, take/drop, locked-exit item use, NPC dialogue, inventory, save/load, help, and quit.
- Local Zoo-Keeper integration with prompt assembly, scoped NPC tool policy, validated tool calls, explicit `remember` memory entries, and mutation-gated state changes.
- Stub dialogue mode for empty `model_path`, plus timeout/failure recovery for model-backed dialogue.
- Google Test coverage for engine, validator, prompt contract, persistence, tools, CLI, and rendering behavior.

### Notes

- Chronicle remains fully offline. Model-backed tests are opt-in through `ZOO_INTEGRATION_MODEL` or `ZOO_MODEL_PATH`.
- C++ APIs are implementation details for v1; the stable author-facing surface is the CLI plus JSON package schema.
