# Chronicle Scenario Authoring Guide

This guide walks a new author through building a Chronicle scenario package
end-to-end. In product terms, Chronicle is the console/runtime and a scenario
package is a cartridge: portable authored content that the console validates,
loads, saves against, and runs. The core walkthrough snippets are taken
directly from the bundled sample at [`data/`](../data) so you can compare the
guide to a working package as you read.

For the smallest copyable starting point, use
[`examples/minimal_scenario/`](../examples/minimal_scenario).

Chronicle's v1 public contract is the CLI plus the JSON scenario package
schema. C++ APIs are not part of the contract. The C++ sources can help
explain current implementation behavior, but the author-facing contract is
the CLI, this guide, the JSON schemas, and validator diagnostics.

For a concise field-by-field reference, see
[`docs/scenario-package-schema.md`](scenario-package-schema.md).

## 1. Overview

A **scenario package** is Chronicle's cartridge format: a directory of JSON
files that fully describes one mystery / social-sim adventure, including its
world, NPCs, facts, flags, scripted events, and runtime configuration.

A package contains exactly **seven files**: a manifest plus six required data
files referenced from the manifest.

| File            | Purpose                                                       |
| --------------- | ------------------------------------------------------------- |
| `scenario.json` | Cartridge manifest. Names the package and points at the six data files. |
| `config.json`   | Runtime tuning: turn pacing, narration templates, budgets.    |
| `world.json`    | Locations (graph), items, the player's start location.        |
| `npcs.json`     | NPC identity, initial state, per-NPC tool policy.             |
| `facts.json`    | Author-declared facts NPCs can disclose.                      |
| `flags.json`    | Boolean narrative flags with default values.                  |
| `events.json`   | Deterministic scripted triggers (conditions + actions).       |

The manifest declares a `chronicle_schema_version`. The current runtime
accepts version `1` and refuses any other value (`kCurrentScenarioSchemaVersion`
in [`src/entities/scenario.hpp`](../src/entities/scenario.hpp)).

A complete `scenario.json` from the sample package:

```json
{
  "id": "broken_wheel_sample",
  "name": "Broken Wheel Sample",
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
    "description": "A small NPC mystery/social-sim sample cartridge for the Chronicle runtime.",
    "author": "Chronicle Contributors",
    "license": "MIT"
  }
}
```

`metadata` is an optional string-to-string map (description, author, license,
etc.). The runtime does not interpret it.

## 2. Project layout

A package is just a directory. The simplest layout matches the sample:

```
my_scenario/
├── scenario.json
├── config.json
├── world.json
├── npcs.json
├── facts.json
├── flags.json
└── events.json
```

`files.*` paths are **relative to the package directory** and must stay
inside it. Absolute paths and `..` escapes are rejected by
`load_scenario_package` ([`src/entities/scenario.cpp`](../src/entities/scenario.cpp)).
Subdirectories are allowed (`"world": "world/world.json"`).

If you are starting from scratch, copy `examples/minimal_scenario/` and rename
the package fields before expanding the world.

### Create a cartridge workflow

1. Copy `examples/minimal_scenario/` to a new directory.
2. Change `scenario.json` fields: `id`, `name`, `version`, and recommended
   `metadata.description`, `metadata.author`, and `metadata.license`.
3. Keep `config.json` portable: leave endpoint fields empty and put local
   endpoint settings in operator overrides only.
4. Run `chronicle inspect --scenario path/to/my_scenario` to review identity,
   file layout, and readiness warnings.
5. Run `chronicle validate --scenario path/to/my_scenario` as the model-free
   gate before sharing.
6. Run with stub dialogue first, then add local endpoint overrides for
   AI-backed playtesting.

**Entity IDs come from JSON map keys.** When you author `npcs.json`,
`world.json`'s `locations`/`items`, `facts.json`, `flags.json`, or
`events.json`, the key in the map *is* the entity's ID. The world loader
injects that key into the entity object after parsing — do not repeat the
`id` field inside the object body. See `load_world` in
[`src/entities/world_loader.cpp`](../src/entities/world_loader.cpp).

Pick stable, lowercase, snake_case IDs (`tavern`, `marcus`,
`fact_thief_identity`, `cargo_inquiry_public`).

## 3. `config.json`

`config.json` contains scenario-authored defaults. Chronicle applies
machine-local operator overrides after loading this file, so committed packages
should stay portable.

**Authored fields** — committed with the package:

- `turns_per_period` (int): player actions per in-game time period.
- `total_periods` (int): when the clock reaches this many elapsed periods
  without an authored `end_game` event, the runtime ends the scenario with a
  generic time-expired message (`12` = three days at four periods each).
- `max_response_tokens`, `temperature`: model generation settings.
- `inference_timeout_ms` (int, default `120000`): wall-clock timeout for one
  model request. Set to `0` only when debugging a local model and you want to
  disable cancellation.
- `max_memory_tokens`, `max_world_tokens`, `max_history_tokens`: prompt-budget
  caps `PromptBuilder` enforces.
- `mutation_narration_templates` (object): per-mutation narration templates.
  Keys are mutation type names; values may include `{npc}`, `{item}`,
  `{mood}`, `{location}` placeholders. Empty string suppresses narration for
  that mutation.
- `use_tui`, `use_color`: renderer preferences.

**Operator-supplied fields** — leave empty or generic in committed packages:

- `llm_base_url` (string): base URL for an OpenAI-compatible endpoint.
- `llm_model` (string): model identifier passed to that endpoint.
- `llm_api_key` (string): endpoint credential. Never commit real keys.
- `llm_organization` (string): optional organization header value.
- `llm_http_timeout_ms`, `llm_max_retries`, `llm_tls_verify`: transport
  settings for the harness.
- `save_directory` (string): where save files are written. Empty falls back
  to `saves` relative to the working directory.

Keep machine-local values out of committed packages. See
[`CONTRIBUTING.md`](../CONTRIBUTING.md) "Local LLM Endpoints" for the override
mechanism (`CHRONICLE_CONFIG_OVERRIDE`, environment variables, and gitignored
local config).

The sample's `config.json`:

```json
{
  "temperature": 0.7,
  "max_response_tokens": 512,
  "inference_timeout_ms": 120000,
  "turns_per_period": 5,
  "total_periods": 12,
  "max_memory_tokens": 800,
  "max_world_tokens": 400,
  "max_history_tokens": 600,
  "save_directory": "",
  "use_tui": false,
  "use_color": true,
  "mutation_narration_templates": {
    "give_item_to_player": "{npc} hands you the {item}.",
    "take_item_from_player": "{npc} takes the {item}.",
    "update_npc_mood": "{npc}'s expression shifts - they seem {mood} now.",
    "move_npc": "{npc} excuses themselves and leaves.",
    "reveal_knowledge": "",
    "update_npc_trust": "",
    "add_memory": "",
    "set_flag": ""
  }
}
```

## 4. `world.json`

Declares the spatial graph, the item registry, and the player's starting
location.

```json
{
  "start_location": "tavern",
  "locations": {
    "tavern": {
      "name": "The Broken Wheel Tavern",
      "base_description": "A dimly lit tavern with creaking floorboards and the smell of stale ale. A cracked wagon wheel hangs above the bar as a sign.",
      "exits": { "north": "market_square" },
      "items": ["crumpled_note"],
      "npcs": [],
      "locked_exits": []
    },
    "market_square": {
      "name": "Market Square",
      "base_description": "A wide cobblestone square surrounded by merchant stalls and trade offices. The air buzzes with haggling voices.",
      "exits": { "south": "tavern" },
      "items": [],
      "npcs": [],
      "locked_exits": []
    }
  },
  "items": {
    "crumpled_note": {
      "name": "Crumpled Note",
      "description": "A hastily written note on stained parchment.",
      "takeable": true,
      "key_item": false,
      "hidden": false,
      "unlock_target": "",
      "properties": {
        "readable": "true",
        "text": "Don't trust the innkeeper."
      }
    },
    "tavern_key": {
      "name": "Tavern Key",
      "description": "A heavy iron key with a broken wheel emblem on the bow.",
      "takeable": true,
      "key_item": true,
      "hidden": false,
      "unlock_target": "",
      "properties": {}
    },
    "cargo_manifest": {
      "name": "Cargo Manifest",
      "description": "A detailed list of goods, quantities, and destinations. Several entries are crossed out.",
      "takeable": true,
      "key_item": true,
      "hidden": false,
      "unlock_target": "",
      "properties": {
        "readable": "true",
        "text": "Shipment 47: 12 bolts silk, 3 casks Elvari wine, 1 sealed chest (CONTENTS UNKNOWN). Status: MISSING."
      }
    }
  }
}
```

Location fields:

- `exits`: maps direction strings (`north`, `upstairs`, anything) to
  destination location IDs. Validation rejects unknown targets.
- `locked_exits`: list of direction strings (keys from `exits`) that start
  locked. A player cannot traverse a locked exit until an inventory item with
  a matching `unlock_target` is used on that direction, destination ID, or
  destination display name.
- `items`, `npcs`: entities initially present. NPC starting locations are
  driven by `state.current_location` in `npcs.json`; the loader cross-
  references those into the location's `npcs` list automatically. Leave the
  location's `npcs` empty unless overriding.

Item fields:

- `takeable`: whether the player can pick it up.
- `key_item`: plot-critical. `take_item` refuses to take key items back.
- `hidden`: omitted from ambient room descriptions; must be revealed
  explicitly.
- `unlock_target`: destination location ID this item opens when used on a
  matching locked exit. Empty for items with no unlock effect.
- `properties`: extensible string-to-string metadata. The runtime currently
  recognises `readable: "true"` paired with `text` to render document
  contents. The validator warns when `readable=true` is set but `text` is
  missing or empty.

**Item ownership is unique.** A given item ID may appear in *at most one*
container across the entire world (a single location's `items`, a single
NPC's `state.inventory`, or the player's inventory). Duplicates are
validation errors.

## 5. `npcs.json`

NPCs split into two halves: an immutable `identity` and a mutable `state`.
Identity is loaded once and never changes; state evolves as the mutation
pipeline applies validated tool calls.

```json
{
  "npcs": {
    "marcus": {
      "identity": {
        "id": "marcus",
        "name": "Marcus",
        "role": "Innkeeper",
        "personality_summary": "A tired, guarded man in his fifties who runs the Broken Wheel Tavern. He speaks carefully, avoiding eye contact when lying.",
        "backstory": "Marcus witnessed the theft of a merchant's cargo but said nothing. He helped the thief escape through the tavern's back entrance in exchange for a share of the stolen goods.",
        "secret": "He helped the thief escape in exchange for coin.",
        "goals": ["Protect his daughter Elena", "Avoid implicating himself", "Keep the tavern running"],
        "knowledge": ["fact_stolen_cargo", "fact_thief_identity"],
        "trust_reveal_threshold": 65,
        "tool_policy": {
          "allowed_tools": [
            "say", "give_item", "take_item", "update_mood", "update_trust",
            "move_self", "reveal_knowledge", "remember", "set_flag", "inspect_item"
          ],
          "allowed_items": ["tavern_key", "cargo_manifest", "crumpled_note"],
          "allowed_facts": ["fact_stolen_cargo", "fact_thief_identity"],
          "allowed_flags": ["cargo_inquiry_public"],
          "allowed_locations": ["tavern", "market_square"]
        }
      },
      "state": {
        "current_location": "tavern",
        "mood": "neutral",
        "trust_toward_player": 0,
        "inventory": ["tavern_key", "cargo_manifest"],
        "memories": [
          {
            "timestamp": "Night, Day 0",
            "type": "observation",
            "summary": "A dockworker argued with a merchant about a sealed chest on the night the cargo vanished.",
            "importance": 6,
            "related_npc": "",
            "related_item": "cargo_manifest"
          }
        ],
        "has_met_player": false,
        "secret_revealed": false
      }
    }
  }
}
```

`identity` fields:

- `personality_summary`: free-form prose injected into the LLM system
  prompt. This is what makes the NPC feel like a character.
- `backstory`: private prose only the NPC knows; always present in the
  system prompt.
- `secret`: the one thing the NPC is hiding. **Only injected into the prompt
  when `state.trust_toward_player >= identity.trust_reveal_threshold`.**
  This is the central trust gate for mystery scenarios.
- `goals`: list of narrative objectives the NPC is pursuing.
- `knowledge`: list of fact IDs the NPC knows. Every ID must exist in
  `facts.json`. `reveal_knowledge` refuses to disclose any fact not in this
  list.
- `trust_reveal_threshold`: int in roughly `[-100, 100]`. Default 70.

`state` fields:

- `current_location`: NPC's starting location. Must reference a real
  location.
- `mood`: one of the six valid moods (below).
- `trust_toward_player`: int in `[-100, 100]`; clamped by the mutation
  pipeline.
- `inventory`: item IDs initially held. Subject to the unique-ownership
  invariant.
- `memories`: optional authored seed memories plus runtime entries from
  explicit `remember` tool calls. Chronicle 1.0 does not run automatic memory
  extraction after conversations.
- `has_met_player`, `secret_revealed`: maintained by the engine.

**Valid moods** (`kValidMoods`): `fearful`, `friendly`, `grieving`,
`hostile`, `neutral`, `suspicious`. `update_mood` rejects anything else.

### `tool_policy`

`tool_policy` is the per-NPC allowlist enforced *before* normal world
validation. If a policy check fails, the tool call is rejected and an error
is returned to the model — the world is never touched.

- `allowed_tools` — framework tools this NPC may invoke. **Empty means the
  NPC has no tool permissions.** A missing field defaults to the full v1
  palette (`default_allowed_npc_tools` in
  [`src/entities/npc.hpp`](../src/entities/npc.hpp)).
- `allowed_items`, `allowed_facts`, `allowed_flags`, `allowed_locations` —
  scoped allowlists. **An empty scoped list means "no additional ID
  restriction."** Populate it to restrict (e.g. `allowed_items: ["tavern_key"]`
  lets the NPC only ever give/take/inspect that one item).

The validator rejects policies that name unknown tools or reference IDs that
do not exist.

## 6. `facts.json`

Facts are author-declared knowledge records. NPCs reference facts by ID in
`identity.knowledge` and disclose them via `reveal_knowledge`. Facts never
mutate at runtime.

```json
{
  "facts": {
    "fact_stolen_cargo": {
      "text": "A merchant's cargo was stolen from the docks last week. The goods were never recovered.",
      "category": "backstory",
      "revealed_by_default": false
    },
    "fact_thief_identity": {
      "text": "The thief was a dockworker named Harlan who fled upriver after the job.",
      "category": "backstory",
      "revealed_by_default": false
    }
  }
}
```

- `text`: prose the model sees when an NPC reveals or reasons about the
  fact.
- `category`: authoring label (`"backstory"`, `"clue"`, `"rumor"`...). The
  runtime preserves the field but does not currently branch on it.
- `revealed_by_default`: when `true`, the fact is added to the player's
  `known_facts` during world load.

Authoring rule: every fact ID listed in any NPC's `knowledge` or any
`tool_policy.allowed_facts` must exist in `facts.json`.

## 7. `flags.json`

Flags are author-declared boolean milestones used by NPC tools and event
conditions. The sample declares the public cargo inquiry flag that its first
scripted event sets:

```json
{
  "flags": {
    "cargo_inquiry_public": {
      "default": false,
      "description": "Set when the market guards publicly start questioning traders about the missing cargo."
    }
  }
}
```

- `default`: initial value loaded into the runtime's flag state.
- `description`: authoring note. Not currently surfaced at runtime; useful
  for keeping a registry of what each flag means.

NPCs set flags via `set_flag` (subject to `allowed_flags` scoping). Events
read flags via the `flag_set` condition. Validation rejects unknown flag IDs
in condition args, `set_flag` actions, and `allowed_flags` lists.

## 8. `events.json`

Events are deterministic, LLM-independent scripted triggers. Each turn,
after mutations are applied, the engine evaluates every unfired trigger.
When all conditions are true (AND semantics), the actions run.

```json
{
  "events": {
    "market_commotion": {
      "conditions": [
        {"type": "clock_is", "args": ["afternoon"]},
        {"type": "player_at", "args": ["market_square"]}
      ],
      "actions": [
        {
          "type": "set_flag",
          "params": {
            "flag_id": "cargo_inquiry_public",
            "value": "true"
          }
        },
        {
          "type": "narrate",
          "params": {
            "text": "A commotion breaks out near the merchant stalls. Guards are questioning traders about the missing cargo."
          }
        }
      ],
      "once": true,
      "fired": false
    }
  }
}
```

`once: true` (the default) disables the trigger after firing. `fired` is
runtime state — author it as `false`.

The bundled sample also contains `cargo_trail_goes_cold`, an `end_game` event
that fires once `cargo_inquiry_public` is true and enough turns have elapsed.
That is the smallest deterministic resolution pattern: one event sets a flag,
and a later event ends the scenario when the flag and pacing condition match.

`once: false` keeps `fired` as `false` and lets the trigger fire on every
post-turn evaluation where its conditions remain true. Use an authored flag
condition/action if a repeating event needs its own cooldown or gate.

Condition types validated by `validate_world` in
[`src/entities/world_validator.cpp`](../src/entities/world_validator.cpp):

| `type`              | `args`                              | Meaning                                                            |
| ------------------- | ----------------------------------- | ------------------------------------------------------------------ |
| `clock_is`          | `[period]`                          | `period` is one of `morning`, `afternoon`, `evening`, `night`.     |
| `player_at`         | `[location_id]`                     | Player's `current_location` matches.                               |
| `flag_set`          | `[flag_id, "true"\|"false"]`        | Declared flag has the given value.                                 |
| `npc_trust_ge`      | `[npc_id, threshold_int]`           | NPC's `trust_toward_player` is `>= threshold`.                     |
| `npc_at`            | `[npc_id, location_id]`             | NPC's `current_location` matches.                                  |
| `item_in_player_inv`| `[item_id]`                         | Player inventory contains the item.                                |
| `turn_ge`           | `[non_negative_int]`                | `total_turns_elapsed >= threshold`.                                |

All `args` are strings; the engine parses ints and bools.

Action types:

| `type`       | `params`                                 | Effect                                       |
| ------------ | ---------------------------------------- | -------------------------------------------- |
| `move_npc`   | `npc_id`, `location_id`                  | Teleport the NPC.                            |
| `set_flag`   | `flag_id`, `value` (`"true"`/`"false"`)  | Set a declared flag.                         |
| `spawn_item` | `item_id`, `location_id`                 | Place an existing item ID into a location.   |
| `narrate`    | `text`                                   | Print narration to the player.               |
| `end_game`   | optional `text`                          | Trigger the resolution pathway.              |

Unknown action types are validation errors. Required `params` keys must be
non-empty strings.

### Chained example: a flag-driven follow-up event

Two triggers, the second gated on the first having fired:

```json
{
  "events": {
    "marcus_confesses": {
      "conditions": [
        {"type": "npc_trust_ge", "args": ["marcus", "65"]},
        {"type": "player_at", "args": ["tavern"]}
      ],
      "actions": [
        {"type": "set_flag", "params": {"flag_id": "secret_revealed", "value": "true"}},
        {"type": "narrate", "params": {"text": "Marcus's shoulders sag. He looks ready to talk."}}
      ],
      "once": true,
      "fired": false
    },
    "guards_arrive": {
      "conditions": [
        {"type": "flag_set", "args": ["secret_revealed", "true"]},
        {"type": "turn_ge", "args": ["10"]}
      ],
      "actions": [
        {"type": "spawn_item", "params": {"item_id": "warrant_scroll", "location_id": "tavern"}},
        {"type": "narrate", "params": {"text": "Boots on the cobblestones. Two guards push through the door."}}
      ],
      "once": true,
      "fired": false
    }
  }
}
```

This pattern — event sets a flag, a later event reads it — is how scenarios
sequence narrative beats deterministically. (To validate, this snippet also
needs `secret_revealed` declared in `flags.json` and `warrant_scroll` declared
in `world.json`'s `items`.)

## 9. NPC tool palette reference

Each tool that mutates state produces a single `MutationRequest` that flows
through the engine's mutation gate. Validation logic lives in `validate_*`
methods in [`src/ai/tool_registry.cpp`](../src/ai/tool_registry.cpp).

| Tool               | Args                              | Mutation produced            | Notes                                                                       |
| ------------------ | --------------------------------- | ---------------------------- | --------------------------------------------------------------------------- |
| `say`              | `dialogue`                        | (none — appended to log)     | Always allowed if `say` is in `allowed_tools`.                              |
| `give_item`        | `item_id`                         | `GiveItemToPlayer`           | Item must be in NPC inventory.                                              |
| `take_item`        | `item_id`                         | `TakeItemFromPlayer`         | Refuses key items.                                                          |
| `update_mood`      | `mood`                            | `UpdateNpcMood`              | `mood` must be in `kValidMoods`.                                            |
| `update_trust`     | `delta`                           | `UpdateNpcTrust`             | Delta clamped so trust stays in `[-100, 100]`.                              |
| `move_self`        | `location_id`                     | `MoveNpc`                    | Location must exist and be in `allowed_locations` if scoped.                |
| `reveal_knowledge` | `fact_id`                         | `RevealKnowledge`            | Fact must be in NPC's `identity.knowledge`.                                 |
| `remember`         | `summary`, `importance` (1-10)    | `AddMemory`                  | Durable future-relevant memory only; summary non-empty; importance clamped to `[1, 10]`. |
| `set_flag`         | `flag_id`, `value`                | `SetFlag`                    | Flag must be declared in `flags.json`.                                      |
| `inspect_item`     | `item_id`                         | (none — read-only response)  | Item must be in NPC inventory; returns description and properties.          |

Every tool also enforces the active NPC's `tool_policy`: tool name in
`allowed_tools`, and the relevant scoped allowlist (`allowed_items`,
`allowed_facts`, `allowed_flags`, `allowed_locations`) if non-empty.

## 10. Validating your package

Inspect cartridge identity and readiness:

```bash
chronicle inspect --scenario path/to/my_scenario
```

Run validation without starting play:

```bash
chronicle validate --scenario path/to/my_scenario
```

This is the model-free entry point. The validator catches:

- **Missing files** — manifest lists a path that doesn't exist or isn't a
  regular file.
- **Schema mismatch** — `chronicle_schema_version` differs from the runtime's
  supported version (currently `1`).
- **Manifest path escapes** — absolute paths or relative paths that resolve
  outside the package root.
- **Dangling cross-references** — locations, NPCs, items, facts, flags, or
  exits referenced by ID but not declared. Includes player start location,
  NPC `current_location`, location `exits` targets, NPC `knowledge` fact IDs,
  and event condition/action references.
- **Tool-policy IDs that don't exist** — any tool, item, fact, flag, or
  location named in an NPC's `tool_policy` but missing from the corresponding
  registry, or any tool name not in `kValidNpcTools`.
- **Event condition/action arg shapes** — wrong number of `args`, unknown
  condition/action types, non-boolean flag values, non-integer thresholds,
  unknown time periods, missing required `params` keys.
- **Item ownership uniqueness** — the same item ID appearing in more than one
  of: a location, an NPC inventory, the player's inventory.

The validator also emits warnings (which do not block load) — for example,
`readable=true` items missing a `text` property, missing recommended cartridge
metadata, suspicious manifest IDs or versions, or a committed non-empty
LLM endpoint settings.

Before sharing a cartridge, check:

- Stable lowercase package ID with no whitespace or path separators.
- Content version such as `1.0.0`.
- `metadata.description`, `metadata.author`, and `metadata.license`.
- Empty `llm_base_url`, `llm_model`, and `llm_api_key`; endpoint settings stay
  in operator overrides.
- Valid cross-references for locations, NPCs, items, facts, flags, and events.
- Scoped NPC tool policies where appropriate.
- Clean validation, with any warnings intentionally resolved or accepted.

## 11. Running your package

```bash
chronicle --scenario path/to/my_scenario
```

Without an explicit `--scenario`, Chronicle runs the bundled `data/` sample.

If `llm_base_url` or `llm_model` is empty and the operator has not provided
them through a local override, the runtime falls back to **stub dialogue** —
NPCs respond with placeholder text, scripted events still fire, and the rest of
the engine continues to work. Real AI dialogue requires an OpenAI-compatible
endpoint. See the "Local LLM Endpoints" section of
[`CONTRIBUTING.md`](../CONTRIBUTING.md) for override mechanisms
(`CHRONICLE_CONFIG_OVERRIDE`, `CHRONICLE_LLM_BASE_URL`, `ZOO_BASE_URL`, and
gitignored local config).

If a model request exceeds `inference_timeout_ms`, Chronicle requests harness
cancellation, discards NPC tool mutations from that failed turn, and
keeps deterministic commands such as save, load, help, and leaving the
conversation available. Set `inference_timeout_ms` to `0` only for endpoint
debugging sessions where cancellation would hide the issue you are inspecting.

## 12. Where to look next

- [`data/`](../data) — canonical small sample package. Read it end-to-end
  before authoring your own.
- [`examples/minimal_scenario/`](../examples/minimal_scenario) — smallest
  copyable starter package.
- [`examples/lighthouse_veil/`](../examples/lighthouse_veil) — richer
  reference package showing multi-location authoring, scoped NPC policies, and
  event chains.
- [`schemas/`](../schemas) — JSON Schema files for each scenario file, useful
  for editor integration.
- [`docs/scenario-package-schema.md`](scenario-package-schema.md) — concise
  field-by-field package contract reference.
- [`src/entities/`](../src/entities) — implementation code for the current
  loader and validator. Treat it as explanatory, not as a stable public API.
- [`src/entities/world_validator.cpp`](../src/entities/world_validator.cpp) —
  exact, exhaustive list of validation rules.
- [`src/ai/tool_registry.cpp`](../src/ai/tool_registry.cpp) — exact tool
  signatures and validation behaviour.
