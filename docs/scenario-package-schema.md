# Scenario Package Schema

A Chronicle cartridge is a directory containing `scenario.json` and six data files.

## Manifest (`scenario.json`)

| Field | Type | Notes |
| --- | --- | --- |
| `id` | string | Stable id matching `[a-z0-9][a-z0-9_-]{0,63}` |
| `name` | string | Display name |
| `version` | string | Cartridge content version |
| `chronicle_schema_version` | int | Must be `1` |
| `files` | object | Relative paths to the six data files |
| `metadata` | object | Optional `description`, `author`, `license` |

Paths must stay inside the package directory (`..`, absolute paths, backslashes, and symlinks are
rejected). A package may contain at most 256 files, 4 MiB per file, and 32 MiB total.

## Config (`config.json`)

Optional; empty object is valid. Common fields:

- Clock: `turns_per_period`, `total_periods`
- Prompt budgets: `max_memory_tokens`, `max_world_tokens`, `max_history_tokens`
- `verb_aliases`, `mutation_narration_templates`
- Inference: `temperature`, `max_response_tokens`, `inference_timeout_ms`

Network endpoints, API credentials, and save locations are host policy. Cartridges cannot set
them; use the CLI or `CHRONICLE_*` environment variables for model configuration.

## World (`world.json`)

- `start_location`
- `locations`: id → name, description, exits, items, locked_exits
- `items`: id → name, description, takeable, key_item, hidden, unlock_target, properties

## NPCs (`npcs.json`)

Map of npc id → `{ identity, state }`.

Identity includes persona fields, `knowledge` fact ids, `secret`,
`trust_reveal_threshold`, and `tool_policy`.

State includes location, mood, trust, inventory, memories, flags for met/secret.

## Facts / Flags / Events

- `facts.json`: authored knowledge the model may reveal via `reveal_knowledge`
- `flags.json`: boolean narrative flags with defaults
- `events.json`: triggers with AND conditions and actions

See [`console-api.md`](console-api.md) for condition/action vocabularies.
