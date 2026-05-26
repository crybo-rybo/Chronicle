# Schema v2 Mechanics Plan

This document drafts the next cartridge schema expansion for Chronicle. Schema
v2 remains **data-only**: new mechanics are declared in JSON and executed through
the existing mutation + event pipeline. No runtime plugins.

## Goals

- Support broader game types (economy, inventory depth, light combat) within one
  console runtime
- Preserve the single mutation gate and validated tool/event vocabularies
- Allow schema v1 cartridges to keep running unchanged

## Versioning Policy

- `chronicle_schema_version: 1` — current mystery/social-sim palette
- `chronicle_schema_version: 2` — adds optional mechanics blocks below
- Runtime accepts v1 and v2; v2-only fields are ignored when schema is v1
- Save files gain optional `mechanics_version` metadata in schema v2

## Proposed World Extensions

### Economy (`world.json` + `Player`)

```json
{
  "player_start": {
    "location": "market",
    "gold": 25
  }
}
```

| Addition | Type | Notes |
| --- | --- | --- |
| `player.gold` | integer | Wallet balance, default 0 |
| `item.price` | integer | Optional shop price when `properties.shop` is set |

New player verbs: `buy <item>`, `sell <item>`.

New mutations: `AdjustPlayerGold`, `TransferItemShop`.

New event action: `adjust_currency` with `delta`.

### Inventory depth (`items` + player commands)

| Field | Type | Notes |
| --- | --- | --- |
| `stackable` | boolean | Allow quantity > 1 |
| `quantity` | integer | Runtime stack size in containers |
| `equippable` | boolean | May occupy equipment slot |

New player verbs: `equip <item>`, `unequip <slot>`.

New mutations: `SetItemQuantity`, `EquipItem`, `UnequipItem`.

### Text combat (`npcs.json` state + verbs/tools)

| Field | Type | Notes |
| --- | --- | --- |
| `player.hp` / `player.max_hp` | integer | Optional combat track |
| `npc.state.hp` | integer | Per-NPC health when combat enabled |

New player verb: `attack <npc>`.

New NPC tool: `damage_player` (validated, bounded delta).

New event condition: `npc_hp_le` with `npc_id`, `threshold`.

New mutations: `DamageNpc`, `DamagePlayer`, `HealPlayer`.

### Quests (new optional `quests.json`)

Manifest v2 may declare `files.quests`. Quest objects track staged objectives
linked to flags and events:

```json
{
  "quests": {
    "find_the_key": {
      "title": "Find the Key",
      "stages": [
        { "id": "talk_innkeeper", "complete_when": { "flag_set": ["met_innkeeper", "true"] } }
      ]
    }
  }
}
```

Runtime exposes quest state in prompts; completion is event-driven (no scripts).

### Branching endings

Extend `end_game` action:

```json
{ "type": "end_game", "params": { "outcome": "win", "text": "Case closed." } }
```

`outcome`: `win`, `lose`, `neutral`. Renderer and save summary may display outcome.

## Implementation Tracks

| Track | Scope |
| --- | --- |
| Delta / schema-v2-foundation | Schema types, validator stubs, feature flags |
| Delta / economy | Gold, buy/sell, shop items |
| Delta / inventory-v2 | Quantities, equip slots |
| Delta / combat-text | HP, attack verb, damage tools |
| Delta / quests | Optional quests file + prompt injection |

Each track ships with fixture cartridges and Console API doc updates.

## Non-Goals for v2

- Native plugins or embedded scripting
- Runtime item creation beyond pre-declared registry + `spawn_item`
- Online/cloud inference

## Compatibility Checklist

Before merging schema v2:

- [ ] v1 cartridges validate and run unchanged
- [ ] v2 fixtures cover each new mechanic minimally
- [ ] `docs/console-api.md` updated
- [ ] JSON schemas under `schemas/` updated
- [ ] Save/load round-trips new player/NPC fields
- [ ] Tool/event validators reject unknown v2 types in v1 packages
