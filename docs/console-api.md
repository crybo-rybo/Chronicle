# Chronicle Console API

This document is the creator-facing catalog of mechanics exposed by the
Chronicle runtime. Cartridges may use only the vocabularies listed here;
anything not documented requires a Chronicle engine update (schema version bump).

Chronicle is the console. JSON scenario packages are cartridges. The stable
public contract remains the CLI plus the JSON schema — not C++ headers.

## Player Commands

Available during `GamePhase::Playing`:

| Command | Arguments | Effect |
| --- | --- | --- |
| `go` | `<direction>` | Move through an exit in the current location |
| `look` | — | Re-render the current location |
| `examine` | `<item>` | Show item description when accessible |
| `take` | `<item>` | Pick up a takeable item in the current location |
| `drop` | `<item>` | Drop an inventory item in the current location |
| `use` | `<item> on/with <target>` | Use an item, including authored unlock targets |
| `talk` | `<npc>` | Begin a conversation with a visible NPC |
| `inventory` | — | List carried items |
| `save` | `[slot]` | Save to slot `N` (default `1`) |
| `load` | `[slot]` | Load slot `N` (default `1`) |
| `help` | — | Show phase-appropriate help |
| `quit` | — | Exit the game |

During `GamePhase::InConversation`, free text is forwarded to the active NPC.
Hard commands still available: `look`, `inventory`, `save`, `load`, `help`, `quit`,
and exit phrases (`bye`, `goodbye`, `leave`, `exit conversation`).

`give` is parsed but rejected at runtime; item exchange must happen through NPC
dialogue tools.

### Verb aliases

Authors may define shorthand aliases in `config.json`:

```json
{
  "verb_aliases": {
    "n": "go north",
    "i": "inventory"
  }
}
```

Aliases expand before command parsing. If an alias expands to an unknown command,
the expanded input is handled like any other unknown command.

## NPC Tool Palette

Fixed v1 tools (see `npcs.json` `tool_policy`):

| Tool | Parameters | Mutation |
| --- | --- | --- |
| `say` | `text` | none (display only) |
| `give_item` | `item_id` | `give_item_to_player` |
| `take_item` | `item_id` | `take_item_from_player` |
| `update_mood` | `mood` | `update_npc_mood` |
| `update_trust` | `delta` | `update_npc_trust` |
| `move_self` | `location_id` | `move_npc` |
| `reveal_knowledge` | `fact_id` | `reveal_knowledge` |
| `remember` | `summary`, `importance` | `add_memory` |
| `set_flag` | `flag_id`, `value` | `set_flag` |
| `inspect_item` | `item_id` | none (returns item text) |

Valid moods: `fearful`, `friendly`, `grieving`, `hostile`, `neutral`, `suspicious`.

Per-NPC `tool_policy.allowed_tools` restricts the palette. Scoped lists
(`allowed_items`, `allowed_facts`, `allowed_flags`, `allowed_locations`) further
limit which IDs a tool may reference.

## Event Conditions

All conditions in one trigger use AND semantics.

| Type | Args | Meaning |
| --- | --- | --- |
| `clock_is` | period name | Current time period matches |
| `player_at` | location_id | Player stands in location |
| `flag_set` | flag_id, true/false | Flag value matches |
| `npc_trust_ge` | npc_id, threshold | NPC trust ≥ threshold |
| `npc_at` | npc_id, location_id | NPC stands in location |
| `item_in_player_inv` | item_id | Player carries item |
| `turn_ge` | turn count | Total turns elapsed ≥ count |

## Event Actions

| Type | Params | Effect |
| --- | --- | --- |
| `move_npc` | `npc_id`, `location_id` | Move NPC |
| `set_flag` | `flag_id`, `value` | Set narrative flag |
| `spawn_item` | `item_id`, `location_id` | Place pre-declared unowned item |
| `narrate` | `text` | Render narration |
| `end_game` | optional `text` | End scenario (`GameOver`) |

## Resolution and Time

- Authors typically end scenarios with an `end_game` event action.
- `config.total_periods` also triggers a generic time-expired ending when the
  clock reaches the final period without an authored `end_game` event firing first.
- `config.turns_per_period` controls how many significant actions advance the clock.

## Mutation Narration Templates

`config.mutation_narration_templates` keys match mutation type names:

- `give_item_to_player`
- `take_item_from_player`
- `update_npc_mood`
- `move_npc`
- `reveal_knowledge`
- `update_npc_trust`
- `add_memory`
- `set_flag`

Placeholders: `{npc}`, `{item}`, `{mood}`, `{location}`. Empty strings suppress narration.

## Cartridge Library CLI

| Command | Purpose |
| --- | --- |
| `chronicle list [--library <dir>]` | List installed cartridges |
| `chronicle run <id> [--library <dir>]` | Launch a library cartridge by manifest id |
| `chronicle install <path> [--library <dir>]` | Install a directory or `.chronicle` archive |
| `chronicle pack --scenario <dir>` | Validate and pack a cartridge archive |

Library roots (first match wins for duplicate ids):

1. Explicit `--library <dir>`
2. `CHRONICLE_LIBRARY_PATH`
3. `~/.chronicle/cartridges`
4. `./cartridges`

## Save Slots

When launched with a manifest, saves are stored under
`saves/<scenario_id>/slot_N.json` and tagged with cartridge metadata.
Loading a save from a different cartridge id is rejected.

## Reserved for Future Schema Versions

The following are planned for schema v2 (not available in v1 cartridges):

- Player currency and buy/sell verbs
- Item quantities, stacking, equip/unequip
- Text combat (`attack`, HP tracking)
- Quest objects and branching `end_game` outcomes

See [schema-v2-mechanics.md](schema-v2-mechanics.md) for the design draft.
