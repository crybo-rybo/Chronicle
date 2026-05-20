# Chronicle Scenario SDK Pivot

**Status:** Current project direction as of v1.0.0.

Chronicle is a bounded scenario SDK/runtime for offline, LLM-driven NPC mystery
and social-sim text adventures. It is no longer framed as one fixed game. The
bundled `data/` package is a canonical sample scenario and contract fixture,
while the product surface is the runtime that can load, validate, and run other
scenario packages.

## Public Contract

The stable v1 public contract is intentionally small:

- `chronicle [--scenario <dir>]` runs a scenario package.
- `chronicle validate --scenario <dir>` validates a package without starting play.
- Scenario packages are directories containing `scenario.json` plus JSON data
  files for config, world, NPCs, facts, flags, and events.

C++ headers, library boundaries, and implementation classes are not stable SDK
APIs for v1. Treat them as implementation details unless a future embeddable SDK
milestone explicitly changes that.

## Runtime Shape

The runtime keeps deterministic state in typed C++ data and lets the local LLM
influence the world only through validated tool calls:

- `World` is the durable state source.
- `GameEngine` is the single mutation gate.
- Player commands, NPC tools, and scripted events produce `MutationRequest`
  values before world state changes.
- The AI layer reads world state for prompts and tool validation, but does not
  write authoritative state directly.
- If model loading or inference fails, deterministic commands remain usable and
  NPC dialogue falls back to explicit stub output.

## Scenario Authoring Focus

Chronicle scenarios are expected to be small, inspectable, and socially dense:

- a bounded location graph,
- a small cast of NPCs with goals, secrets, facts, and scoped tool policies,
- authored items, flags, and deterministic events,
- one or more resolution paths driven by validated world state.

The core authoring references are:

- [`docs/scenario-package-schema.md`](scenario-package-schema.md)
- [`docs/scenario-authoring-guide.md`](scenario-authoring-guide.md)
- [`schemas/`](../schemas)
- [`examples/minimal_scenario/`](../examples/minimal_scenario)
- [`examples/lighthouse_veil/`](../examples/lighthouse_veil)

## Relationship To Older Design Docs

The external design documents remain useful historical and architectural
context, but this pivot note supersedes any older language that presents
Chronicle as only a single authored game. When docs conflict, prefer the v1
runtime contract above: CLI plus scenario package schema.
