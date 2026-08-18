# Chronicle Console API

Creator-facing catalog of mechanics. Cartridges may use only the vocabularies
listed here.

## Player Commands

| Command | Arguments | Effect |
| --- | --- | --- |
| `go` | `<direction>` | Move through an exit |
| `look` | — | Re-render the current location |
| `examine` | `<item>` | Show item description when accessible |
| `take` | `<item>` | Pick up a takeable item here |
| `drop` | `<item>` | Drop a carried item |
| `use` | `<item> on/with <target>` | Use an item (including unlock targets) |
| `talk` | `<npc>` | Begin a conversation |
| `inventory` | — | List carried items |
| `save` | `[slot]` | Save (default slot 1) |
| `load` | `[slot]` | Load (default slot 1) |
| `help` | — | Show help |
| `quit` | — | Exit |

In conversation, free text goes to the active NPC. Hard commands still available:
`look`, `inventory`, `save`, `load`, `help`, `quit`, and exit phrases
(`bye`, `goodbye`, `leave`, `exit conversation`).

Verb aliases may be defined in `config.json`.

## NPC Tool Palette

| Tool | Parameters | Effect |
| --- | --- | --- |
| `say` | `text` | Display dialogue |
| `give_item` | `item_id` | Give held item to player |
| `take_item` | `item_id` | Take item from player |
| `update_mood` | `mood` | Change mood |
| `update_trust` | `delta` | Adjust trust |
| `move_self` | `location_id` | Move NPC |
| `reveal_knowledge` | `fact_id` | Reveal authored fact |
| `remember` | `summary`, `importance` | Store memory |
| `set_flag` | `flag_id`, `value` | Set narrative flag |
| `inspect_item` | `item_id` | Read item text (no mutation) |

Moods: `fearful`, `friendly`, `grieving`, `hostile`, `neutral`, `suspicious`.

Per-NPC `tool_policy` restricts tools and scoped IDs. Only allowed tools are
exposed to the model at all; each conversation turn is an agentic loop of up
to 8 tool rounds, and a rejected tool call is returned to the model as an
explicit structured result it can react to (the player sees no rejection).
Tool arguments are strictly validated against generated JSON schemas — unknown
fields, wrong types, and out-of-vocabulary moods are rejected before the gate
runs. A provider failure rolls back the whole tool turn before deterministic
stub dialogue is shown.

## Event Conditions (AND)

| Type | Args |
| --- | --- |
| `clock_is` | period name (`morning`/`afternoon`/`evening`/`night`) |
| `player_at` | location_id |
| `flag_set` | flag_id, true/false |
| `npc_trust_ge` | npc_id, threshold |
| `npc_at` | npc_id, location_id |
| `item_in_player_inv` | item_id |
| `turn_ge` | turn count |

## Event Actions

| Type | Params |
| --- | --- |
| `move_npc` | `npc_id`, `location_id` |
| `set_flag` | `flag_id`, `value` |
| `spawn_item` | `item_id`, `location_id` |
| `narrate` | `text` |
| `end_game` | optional `text` |

## Time

`config.turns_per_period` and `config.total_periods` drive the clock. Reaching
`total_periods` without an authored `end_game` yields a time-expired ending.
