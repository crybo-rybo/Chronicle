# Chronicle Scenario SDK Pivot

Chronicle is pivoting from a single authored game into a bounded open-source scenario SDK/runtime for offline, LLM-driven NPC mystery and social-sim text adventures.

This document supersedes earlier design-doc language that described Chronicle as only one game and not a framework. The current v1 contract is deliberately narrow: authors create scenario packages; Chronicle runs and validates them.

## Public Contract

- The supported public surface is the CLI plus JSON scenario package schema.
- The default command runs the bundled sample package in `data/`.
- `chronicle --scenario <dir>` runs an explicit scenario package.
- `chronicle validate --scenario <dir>` validates the manifest, referenced files, world graph, NPC tool policies, event references, and ownership invariants without starting play.
- C++ APIs and library boundaries remain implementation details for v1.

## Scenario Package

A package directory contains `scenario.json`, plus JSON files for config, world, NPCs, facts, flags, and events.

`scenario.json` includes:

- `id`, `name`, and `version`
- `chronicle_schema_version`
- `files.config`, `files.world`, `files.npcs`, `files.facts`, `files.flags`, and `files.events`
- optional string metadata

All file paths are relative to the package directory and must stay inside it. Chronicle v1 treats the config, world, NPC, facts, flags, and events files as required package inputs.

## Tool Policy

The v1 NPC tool palette is fixed:

`say`, `give_item`, `take_item`, `update_mood`, `update_trust`, `move_self`, `reveal_knowledge`, `remember`, `set_flag`, `inspect_item`

Each NPC may declare a `tool_policy` in `npcs.json`:

- `allowed_tools` controls which framework tools the NPC may use.
- `allowed_items`, `allowed_facts`, `allowed_flags`, and `allowed_locations` optionally restrict authored IDs.
- Missing `tool_policy` defaults to the full built-in palette for backward compatibility.
- Empty scoped ID lists mean no additional ID restriction.

Tool calls are rejected before normal world validation if they violate policy. The engine still applies all accepted changes through the existing mutation gate.

## Scope

V1 supports NPC mystery/social-sim scenarios: locations, items, facts, flags, scripted events, trust/mood, NPC memories, and constrained LLM tool calls.

V1 does not include custom plugin tools, LLM-assisted authoring, arbitrary game genres, or a stable embeddable C++ SDK.
