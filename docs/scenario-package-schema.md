# Chronicle Scenario Package Schema

This is the concise field reference for the Chronicle 1.0 JSON package
contract. The stable public surface is the CLI plus the files described here:

```text
chronicle [--scenario <dir>]
chronicle validate --scenario <dir>
```

C++ headers, class names, and library layout are implementation details. The
JSON Schema files in [`schemas/`](../schemas) provide machine-readable editor
support; this document explains the same author-facing contract in prose.

## Package Rules

A scenario package is a directory containing `scenario.json` plus six JSON data
files: `config.json`, `world.json`, `npcs.json`, `facts.json`, `flags.json`,
and `events.json`.

Manifest file paths are resolved relative to the package directory. Paths must
be relative and must stay inside the package after normalization. Absolute
paths and `..` traversal are invalid. Every referenced file must exist and be a
regular file.

Entity IDs come from JSON object keys. For example, the key `foyer` in
`world.json`'s `locations` object is the location ID. The loader injects that
key into the runtime entity. Do not rely on an `id` field inside the object
body; when it is accepted, it is optional and should match the map key.

## Minimal Valid Package

The following examples together form a valid two-room, one-NPC package.
A copyable starter with these same pieces lives at
[`examples/minimal_scenario/`](../examples/minimal_scenario).

### `scenario.json`

Required fields:

| Field | Type | Rule |
| --- | --- | --- |
| `id` | string | Stable package ID. Non-empty in the 1.0 schema. |
| `name` | string | Human-readable scenario name. Non-empty in the 1.0 schema. |
| `version` | string | Scenario content version, independent from Chronicle's schema version. |
| `chronicle_schema_version` | integer | Must be `1`. |
| `files` | object | Relative paths to the six package data files. |

Optional/defaulted fields:

| Field | Default | Behavior |
| --- | --- | --- |
| `files.config` | `config.json` | Runtime config file. |
| `files.world` | `world.json` | Location, item, and player-start file. |
| `files.npcs` | `npcs.json` | NPC identity, state, and tool policy file. |
| `files.facts` | `facts.json` | Authored facts file. |
| `files.flags` | `flags.json` | Declared flags file. |
| `files.events` | `events.json` | Scripted events file. |
| `metadata` | `{}` | Optional string-to-string author metadata. The runtime does not interpret it. |

Runtime behavior: the manifest is loaded first. Run mode uses it to resolve the
config and world file set. Validate mode resolves the same files without
constructing a model.

Validation behavior: unsupported `chronicle_schema_version`, unsafe paths, and
missing referenced files are errors. `metadata` must be an object whose values
are strings.

Minimal example:

```json
{
  "id": "two_room_demo",
  "name": "Two Room Demo",
  "version": "1.0.0",
  "chronicle_schema_version": 1,
  "files": {
    "config": "config.json",
    "world": "world.json",
    "npcs": "npcs.json",
    "facts": "facts.json",
    "flags": "flags.json",
    "events": "events.json"
  },
  "metadata": {
    "description": "A minimal Chronicle scenario package."
  }
}
```

### `config.json`

Required fields: none. Missing fields receive runtime defaults.

Optional/defaulted fields:

| Field | Type | Default | Runtime behavior |
| --- | --- | --- | --- |
| `model_path` | string | `""` | Empty means no local model is configured; dialogue uses stub behavior. |
| `context_size` | integer | `4096` | Model context window. |
| `n_gpu_layers` | integer | `-1` | GPU layer offload setting; `0` forces CPU-only. |
| `temperature` | number | `0.7` | Dialogue sampling temperature. |
| `max_response_tokens` | integer | `512` | Maximum generated tokens per response. |
| `inference_timeout_ms` | integer | `120000` | Maximum wall-clock time for one model request. `0` disables timeout cancellation for debugging. |
| `turns_per_period` | integer | `5` | Significant actions per time-period transition. |
| `total_periods` | integer | `12` | Authored pacing value available to runtime logic. |
| `max_memory_tokens` | integer | `800` | Prompt budget for NPC memories. |
| `max_world_tokens` | integer | `400` | Prompt budget for world context. |
| `max_history_tokens` | integer | `600` | Prompt budget for conversation history. |
| `save_directory` | string | `""` | Empty falls back to `saves` relative to the working directory. |
| `use_tui` | boolean | `false` | Plain terminal renderer remains the v1 baseline. |
| `use_color` | boolean | `true` | Enables ANSI color where the renderer supports it. |
| `max_tool_iterations` | integer | `5` | Maximum tool-call loop iterations per model request. |
| `mutation_narration_templates` | object | built-in map | String templates keyed by mutation names. Empty strings suppress narration. |

Validation behavior: malformed JSON or invalid field types fail when the
runtime loads config. The JSON Schema documents numeric minimums for editor
validation.

Minimal example:

```json
{
  "model_path": ""
}
```

### `world.json`

Required top-level fields:

| Field | Type | Rule |
| --- | --- | --- |
| `start_location` | string | Must be a key in `locations`. |
| `locations` | object | Map of location ID to location object. |
| `items` | object | Map of item ID to item object. May be empty. |

Location fields:

| Field | Required | Default | Runtime behavior |
| --- | --- | --- | --- |
| `name` | yes | none | Display name. |
| `base_description` | no | `""` | Default room description. |
| `current_description` | no | `""` | Runtime override when non-empty. Usually empty in authored data. |
| `exits` | no | `{}` | Direction string to destination location ID. |
| `items` | no | `[]` | Item IDs initially in this location. |
| `npcs` | no | `[]` | NPC IDs initially listed here. Usually leave empty; the loader inserts NPCs from `npcs.json`. |
| `locked_exits` | no | `[]` | Exit direction names that start locked. Each entry must be a key in `exits`. |

Item fields:

| Field | Required | Default | Runtime behavior |
| --- | --- | --- | --- |
| `name` | yes | none | Display name. |
| `description` | no | `""` | Text shown by `examine`. |
| `takeable` | no | `true` | If false, player take attempts fail. |
| `key_item` | no | `false` | Protected from some transfer/drop behavior. |
| `hidden` | no | `false` | Hidden items are omitted from ambient scene listings. |
| `unlock_target` | no | `""` | Destination location ID unlocked by this item when used on a matching locked exit. |
| `properties` | no | `{}` | String-to-string metadata. `readable: "true"` plus `text` renders document text. |

ID and cross-reference rules: `start_location`, every exit target, every
`locked_exits` entry, every non-empty item `unlock_target`, every listed item
ID, and every listed NPC ID must refer to declared entities. A given item ID may
appear in at most one owner container: one location, one NPC inventory, or the
player inventory in a save.

Validation behavior: dangling references and duplicate item ownership are
errors. `readable=true` without non-empty `text` is a warning.

Minimal example:

```json
{
  "start_location": "foyer",
  "locations": {
    "foyer": {
      "name": "Foyer",
      "base_description": "A narrow entry hall with a cold draft.",
      "exits": { "east": "study" },
      "items": [],
      "npcs": [],
      "locked_exits": []
    },
    "study": {
      "name": "Study",
      "base_description": "A quiet room lined with ledgers.",
      "exits": { "west": "foyer" },
      "items": [],
      "npcs": [],
      "locked_exits": []
    }
  },
  "items": {}
}
```

### `npcs.json`

Required top-level fields:

| Field | Type | Rule |
| --- | --- | --- |
| `npcs` | object | Map of NPC ID to NPC object. May be empty. |

NPC object fields:

| Field | Required | Runtime behavior |
| --- | --- | --- |
| `identity` | yes | Static persona and policy data. |
| `state` | yes | Mutable runtime state serialized in saves. |

Identity fields:

| Field | Required | Default | Runtime behavior |
| --- | --- | --- | --- |
| `name` | yes | none | Display name. |
| `role` | no | `""` | Short role label for prompts. |
| `personality_summary` | no | `""` | Persona prompt text. |
| `backstory` | no | `""` | Private NPC background in the static prompt. |
| `secret` | no | `""` | Included in prompts only after the trust threshold is met. |
| `goals` | no | `[]` | Prompted NPC objectives. |
| `knowledge` | no | `[]` | Fact IDs this NPC can reveal. |
| `trust_reveal_threshold` | no | `70` | Trust score required before `secret` is included in prompt context. |
| `tool_policy` | no | full built-in palette | Per-NPC tool and authored-ID allowlists. |

State fields:

| Field | Required | Default | Runtime behavior |
| --- | --- | --- | --- |
| `current_location` | yes | none | Must reference a location ID. Loader also inserts the NPC into that location. |
| `mood` | no | `""` | Runtime mood string. Authoring should use `neutral`, `suspicious`, `friendly`, `hostile`, `fearful`, or `grieving`. |
| `trust_toward_player` | no | `0` | Trust score. Mutations clamp it to `[-100, 100]`. |
| `inventory` | no | `[]` | Item IDs initially held by the NPC. |
| `memories` | no | `[]` | Explicit `remember` entries persisted in saves. Authors may seed important memories; Chronicle 1.0 does not auto-extract memories. |
| `has_met_player` | no | `false` | Runtime encounter state. Usually false in authored packages. |
| `secret_revealed` | no | `false` | Runtime secret state. Usually false in authored packages. |

Tool policy semantics:

- Missing `tool_policy` defaults to the full built-in palette.
- Missing `allowed_tools` inside a present policy also uses the default palette.
- Empty `allowed_tools` means no tool permissions.
- Empty `allowed_items`, `allowed_facts`, `allowed_flags`, or
  `allowed_locations` means no additional ID restriction for that scope.
- Non-empty scoped lists restrict the authored IDs that policy-covered tools may
  touch.

Built-in NPC tools: `say`, `give_item`, `take_item`, `update_mood`,
`update_trust`, `move_self`, `reveal_knowledge`, `remember`, `set_flag`,
`inspect_item`.

ID and cross-reference rules: `state.current_location` must exist.
`identity.knowledge` and `allowed_facts` must reference facts. Policy item,
flag, and location scopes must reference declared items, flags, and locations.
Inventory item IDs must exist and must not duplicate another owner.

Validation behavior: unknown tool names, missing policy-scoped IDs, missing
locations, missing knowledge facts, and duplicate item ownership are errors.

Minimal example:

```json
{
  "npcs": {
    "warden": {
      "identity": {
        "name": "Warden",
        "role": "Caretaker",
        "personality_summary": "Patient, observant, and precise.",
        "backstory": "The warden keeps records for every visitor.",
        "secret": "",
        "goals": ["Keep the house orderly"],
        "knowledge": [],
        "trust_reveal_threshold": 70,
        "tool_policy": {
          "allowed_tools": ["say", "remember"],
          "allowed_items": [],
          "allowed_facts": [],
          "allowed_flags": [],
          "allowed_locations": []
        }
      },
      "state": {
        "current_location": "foyer",
        "mood": "neutral",
        "trust_toward_player": 0,
        "inventory": [],
        "memories": [],
        "has_met_player": false,
        "secret_revealed": false
      }
    }
  }
}
```

### `facts.json`

Required top-level fields:

| Field | Type | Rule |
| --- | --- | --- |
| `facts` | object | Map of fact ID to fact object. May be empty. |

Fact fields:

| Field | Required | Runtime behavior |
| --- | --- | --- |
| `text` | yes | Prompt text shown when an NPC knows or reveals the fact. |
| `category` | yes | Authoring label such as `clue`, `backstory`, or `rumor`. |
| `revealed_by_default` | yes | Stored with the fact. The current runtime does not automatically add it to player known facts. |

ID and cross-reference rules: fact IDs referenced by NPC knowledge, NPC policy,
or event/tool behavior must be keys in this file.

Validation behavior: unknown referenced facts are errors.

Minimal example:

```json
{
  "facts": {}
}
```

### `flags.json`

Required top-level fields:

| Field | Type | Rule |
| --- | --- | --- |
| `flags` | object | Map of flag ID to flag object. May be empty. |

Flag fields:

| Field | Required | Runtime behavior |
| --- | --- | --- |
| `default` | yes | Initial boolean value assigned at scenario load. |
| `description` | yes | Authoring note. Not surfaced to the player by default. |

ID and cross-reference rules: flags referenced by NPC policy, `flag_set`
conditions, `set_flag` actions, and `set_flag` NPC tools must be declared here.

Validation behavior: unknown referenced flags and non-boolean event flag values
are errors.

Minimal example:

```json
{
  "flags": {}
}
```

### `events.json`

Required top-level fields:

| Field | Type | Rule |
| --- | --- | --- |
| `events` | object | Map of event ID to trigger object. May be empty. |

Event trigger fields:

| Field | Required | Default | Runtime behavior |
| --- | --- | --- | --- |
| `conditions` | yes | none | All conditions use AND semantics. |
| `actions` | yes | none | Executed in listed order when the trigger fires. |
| `once` | no | `true` | One-shot triggers are disabled after firing. |
| `fired` | no | `false` | Runtime state persisted in saves. Author this as false. |

For `once: false`, Chronicle leaves `fired` as `false`; the trigger may fire
again on each later post-turn pass while all conditions remain true.

Condition types:

| Type | Args | Cross-reference rule |
| --- | --- | --- |
| `clock_is` | `[period]` | `period` is `morning`, `afternoon`, `evening`, or `night`. |
| `player_at` | `[location_id]` | Location must exist. |
| `flag_set` | `[flag_id, "true"|"false"]` | Flag must exist. |
| `npc_trust_ge` | `[npc_id, threshold_int]` | NPC must exist; threshold must be an integer. |
| `npc_at` | `[npc_id, location_id]` | NPC and location must exist. |
| `item_in_player_inv` | `[item_id]` | Item must exist. |
| `turn_ge` | `[non_negative_int]` | Threshold must be a non-negative integer. |

Action types:

| Type | Params | Runtime behavior |
| --- | --- | --- |
| `move_npc` | `npc_id`, `location_id` | Moves an NPC through the mutation pipeline. |
| `set_flag` | `flag_id`, `value` | Sets a declared flag through the mutation pipeline. |
| `spawn_item` | `item_id`, `location_id` | Places an existing unowned item through the mutation pipeline. |
| `narrate` | `text` | Renders authored narration. |
| `end_game` | optional `text` | Ends the scenario through the resolution path. |

Validation behavior: wrong argument counts, unknown condition/action types,
missing required params, invalid bool/int strings, and dangling references are
errors. Validate mode loads and checks events but does not execute them.

Minimal example:

```json
{
  "events": {}
}
```

## Compatibility Notes

- Object keys are the canonical IDs for locations, items, NPCs, facts, flags,
  and events.
- JSON files are intentionally permissive about additional properties so future
  tooling can preserve metadata. Unknown fields should not be used for gameplay
  behavior unless Chronicle documents them.
- Model-backed behavior is optional. Empty `model_path` keeps the package
  runnable in stub dialogue mode.
- Save files are versioned separately from scenario packages.
