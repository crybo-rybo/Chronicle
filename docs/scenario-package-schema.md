# Scenario Package Schema

A Chronicle cartridge is a directory containing `scenario.json` and six data files.
Unknown fields are rejected. `chronicle validate --scenario <dir>` is the
authoritative check. JSON Schema files under [`schemas/`](../schemas) are for
editor tooling and cover each cartridge file.

## Manifest (`scenario.json`)

| Field | Type | Notes |
| --- | --- | --- |
| `id` | string | Stable id matching `[a-z0-9][a-z0-9_-]{0,63}` |
| `name` | string | Display name |
| `version` | string | Cartridge content version |
| `chronicle_schema_version` | int | Must be `1` |
| `files` | object | Relative paths to the six data files |
| `metadata` | object | Optional `description`, `author`, `license` |

Paths must stay inside the package directory (`..`, absolute paths, backslashes, and
symlinks are rejected). A package may contain at most 256 files, 4 MiB per file,
and 32 MiB total.

## Config (`config.json`)

Optional; `{}` is valid. Accepted fields:

| Field | Default | Notes |
| --- | --- | --- |
| `turns_per_period` | `5` | Significant actions per clock period |
| `total_periods` | `12` | Periods before a time-expired ending |
| `max_memory_tokens` | `800` | Prompt budget for NPC memories |
| `max_world_tokens` | `400` | Prompt budget for world context |
| `max_history_tokens` | `600` | Prompt budget for conversation history |
| `verb_aliases` | `{}` | Map of alias → canonical player verb |
| `action_narration_templates` | built-in defaults | Player-facing text after an accepted NPC world write |
| `temperature` | `0.7` | Sampling hint used when a host endpoint is configured |
| `max_response_tokens` | `512` | Output token cap sent to the host endpoint |
| `inference_timeout_ms` | `120000` | Per-turn transfer timeout in milliseconds |

Network endpoints, API credentials, and save locations are host policy. Cartridges
cannot set them; use the CLI or `CHRONICLE_*` environment variables for model
configuration.

Prompt budgets are enforced with a deterministic four-bytes-per-token estimate:
world context and player input are UTF-8-safe truncated, while conversation
history drops the oldest complete turns first.

`use_tui` and `use_color` are ignored if present (no TUI in v2; they are not
written back to saves). `mutation_narration_templates` still loads as an alias
for `action_narration_templates`.

### Narration templates

`action_narration_templates` keys match public NPC tool names. Empty string
suppresses narration. Placeholders: `{npc}`, `{item}`, `{mood}`, `{location}`.

| Key | Default |
| --- | --- |
| `give_item` | `{npc} hands you the {item}.` |
| `take_item` | `{npc} takes the {item}.` |
| `update_mood` | `{npc}'s expression shifts - they seem {mood} now.` |
| `move_self` | `{npc} excuses themselves and leaves.` |
| `reveal_knowledge` | (silent) |
| `update_trust` | (silent) |
| `remember` | (silent) |
| `set_flag` | (silent) |

Legacy keys (`give_item_to_player`, `take_item_from_player`, `update_npc_mood`,
`move_npc`, `update_npc_trust`, `add_memory`) are rewritten to the names above
on load. If both forms are present, the canonical key wins.

## World (`world.json`)

- `start_location`
- `locations`: id → `name`, `base_description`, `exits`, `items`, `locked_exits`
- `items`: id → `name`, `description`, `takeable`, `key_item`, `hidden`,
  `unlock_target`, `properties`

`locked_exits` may be a list of direction strings or objects
`{"direction": "...", "unlocked": false}`. Item `properties` is a string map;
readable items typically set `readable` and `text`.

Location `items` lists are initial placement only. At runtime, item location is
owned by a single `item_positions` map assembled from the cartridge.

## NPCs (`npcs.json`)

Map of npc id → `{ identity, state }`.

Identity: `name` (required), optional `id`, `role`, `personality_summary`,
`backstory`, `secret`, `goals`, `knowledge` (fact ids),
`trust_reveal_threshold` (default `70`), and `tool_policy`.

`tool_policy` lists: `allowed_tools`, `allowed_items`, `allowed_facts`,
`allowed_flags`, `allowed_locations`. Only listed tools are registered on the
model. Empty scoped ID lists mean no additional IDs of that kind are allowed.

State: `current_location`, `mood`, `trust_toward_player`, `inventory`,
`memories`, `has_met_player`, `secret_revealed`.

Moods: `fearful`, `friendly`, `grieving`, `hostile`, `neutral`, `suspicious`.

## Facts / Flags / Events

- `facts.json`: `{ "facts": { id: { "text", optional `category`, `revealed_by_default` } } }`
- `flags.json`: `{ "flags": { id: { "default", optional `description` } } }`
- `events.json`: `{ "events": { id: { `conditions`, `actions`, optional `once`, `fired` } } }`

See [`console-api.md`](console-api.md) for condition/action vocabularies.
