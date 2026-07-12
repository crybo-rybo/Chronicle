# Scenario Package Schema

A Chronicle cartridge is a directory containing `scenario.json` and six data files.

## Manifest (`scenario.json`)

| Field | Type | Notes |
| --- | --- | --- |
| `id` | string | Stable cartridge id |
| `name` | string | Display name |
| `version` | string | Cartridge content version |
| `chronicle_schema_version` | int | Must be `1` |
| `files` | object | Relative paths to the six data files |
| `metadata` | object | Optional `description`, `author`, `license` |

Paths must stay inside the package directory (`..` and absolute paths are rejected).

## Config (`config.json`)

Optional; empty object is valid. Common fields:

- Clock: `turns_per_period`, `total_periods`
- Prompt budgets: `max_memory_tokens`, `max_world_tokens`, `max_history_tokens`
- LLM defaults (leave empty in shared cartridges): `llm_base_url`, `llm_model`, `llm_api_key`
- `verb_aliases`, `mutation_narration_templates`
- Inference: `temperature`, `max_response_tokens`, `inference_timeout_ms`

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
