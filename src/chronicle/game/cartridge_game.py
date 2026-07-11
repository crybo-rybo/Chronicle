"""Default cartridge-backed mystery/social-sim game backend."""

from __future__ import annotations

import re
from pathlib import Path

from chronicle.cartridge.loader import load_package
from chronicle.cartridge.models import (
    VALID_MOODS,
    MemoryEntry,
    WorldState,
)
from chronicle.persist import SaveSystem
from chronicle.prompt_builder import build_npc_turn
from chronicle.types import (
    Action,
    ActionSource,
    GameEvent,
    GamePhase,
    ToolCall,
    TurnRequest,
)

EXIT_PHRASES = frozenset({"bye", "goodbye", "leave", "exit conversation"})
HARD_CONVERSATION_COMMANDS = frozenset({"look", "inventory", "help", "quit", "save", "load"})


class CartridgeGame:
    def __init__(
        self,
        package_dir: str | Path,
        *,
        save_dir: str | Path | None = None,
        world: WorldState | None = None,
    ) -> None:
        self.package_dir = Path(package_dir).resolve()
        self.world = world or load_package(self.package_dir)
        self._phase = GamePhase.PLAYING
        self._active_npc: str | None = None
        self._significant = False
        default_saves = (
            save_dir
            or self.world.config.save_directory
            or (Path.cwd() / "saves" / self.world.manifest.id)
        )
        self._saves = SaveSystem(default_saves)

    @property
    def phase(self) -> GamePhase:
        return self._phase

    @property
    def active_npc_id(self) -> str | None:
        return self._active_npc

    def bootstrap(self) -> list[GameEvent]:
        title = self.world.manifest.name
        return [
            GameEvent(kind="title", text=f"— {title} —"),
            *self._look(),
            GameEvent(kind="hint", text="Type 'help' for commands."),
        ]

    def handle_player(self, text: str) -> list[GameEvent]:
        raw = text.strip()
        if not raw:
            return []
        if self._phase == GamePhase.GAME_OVER:
            return [GameEvent(text="The scenario has ended. Type quit to exit.")]

        expanded = self._expand_alias(raw)
        lower = expanded.lower().strip()

        if self._phase == GamePhase.IN_CONVERSATION:
            if lower in EXIT_PHRASES:
                self._phase = GamePhase.PLAYING
                self._active_npc = None
                return [GameEvent(text="You end the conversation.")]
            first = lower.split(maxsplit=1)[0]
            blocked = {"go", "examine", "take", "drop", "use", "talk"}
            if first in HARD_CONVERSATION_COMMANDS or first in blocked:
                # Only hard list is allowed mid-conversation besides free text.
                if first in HARD_CONVERSATION_COMMANDS:
                    return self._dispatch_playing(expanded)
                return [GameEvent(text="Finish the conversation first (say 'bye').")]
            # Free text handled by LLM path.
            return []

        return self._dispatch_playing(expanded)

    def wants_llm_turn(self, text: str) -> bool:
        if self._phase != GamePhase.IN_CONVERSATION or not self._active_npc:
            return False
        lower = text.strip().lower()
        if not lower or lower in EXIT_PHRASES:
            return False
        first = lower.split(maxsplit=1)[0]
        return first not in HARD_CONVERSATION_COMMANDS

    def build_turn(self, player_text: str) -> TurnRequest:
        assert self._active_npc
        return build_npc_turn(self.world, self._active_npc, player_text)

    def interpret_tools(self, calls: list[ToolCall]) -> list[Action]:
        npc_id = self._active_npc or ""
        actions: list[Action] = []
        for call in calls:
            actions.append(
                Action(
                    type=call.name,
                    source=ActionSource.NPC,
                    actor_id=npc_id,
                    params=dict(call.arguments),
                )
            )
        return actions

    def validate_action(self, action: Action) -> tuple[bool, str]:
        if action.source == ActionSource.NPC:
            return self._validate_npc_tool(action)
        if action.source in {ActionSource.PLAYER, ActionSource.SYSTEM, ActionSource.EVENT}:
            return True, ""
        return False, "Unknown action source"

    def apply(self, actions: list[Action]) -> list[GameEvent]:
        events: list[GameEvent] = []
        for action in actions:
            events.extend(self._apply_one(action))
        return events

    def after_turn(self) -> list[GameEvent]:
        events: list[GameEvent] = []
        if self._significant and self._phase != GamePhase.GAME_OVER:
            self.world.clock.turns_elapsed += 1
            self._significant = False
            if self.world.clock.time_expired():
                self._phase = GamePhase.GAME_OVER
                events.append(
                    GameEvent(
                        kind="ending",
                        text="Time has run out. The scenario ends without a clearer resolution.",
                    )
                )
                return events
        events.extend(self._evaluate_events())
        return events

    def save(self, slot: int) -> None:
        self._saves.save(slot, self.world, phase=self._phase, active_npc=self._active_npc)

    def load(self, slot: int) -> None:
        payload = self._saves.load(slot)
        self.world = WorldState.model_validate(payload["world"])
        self._phase = GamePhase(payload.get("phase", GamePhase.PLAYING))
        self._active_npc = payload.get("active_npc")

    def help_text(self) -> str:
        if self._phase == GamePhase.IN_CONVERSATION:
            return "In conversation: speak freely, or bye/look/inventory/save/load/help/quit."
        return (
            "Commands: go <dir>, look, examine <item>, take/drop <item>, "
            "use <item> on <target>, talk <npc>, inventory, save [n], load [n], help, quit"
        )

    # --- player commands -------------------------------------------------

    def _expand_alias(self, text: str) -> str:
        aliases = self.world.config.verb_aliases
        first, _, rest = text.partition(" ")
        key = first.lower()
        if key in aliases:
            expanded = aliases[key]
            return f"{expanded} {rest}".strip() if rest else expanded
        return text

    def _dispatch_playing(self, text: str) -> list[GameEvent]:
        lower = text.lower().strip()
        parts = lower.split()
        if not parts:
            return []
        cmd = parts[0]
        arg = text.split(maxsplit=1)[1] if len(parts) > 1 else ""

        if cmd in {"quit", "q"}:
            self._phase = GamePhase.GAME_OVER
            return [GameEvent(kind="system", text="Goodbye.")]
        if cmd == "help":
            return [GameEvent(text=self.help_text())]
        if cmd == "look":
            return self._look()
        if cmd == "inventory":
            return self._inventory()
        if cmd == "go":
            return self._go(parts[1] if len(parts) > 1 else "")
        if cmd == "examine":
            return self._examine(arg)
        if cmd == "take":
            return self._take(arg)
        if cmd == "drop":
            return self._drop(arg)
        if cmd == "use":
            return self._use(arg)
        if cmd == "talk":
            return self._talk(arg)
        if cmd == "save":
            slot = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 1
            self.save(slot)
            return [GameEvent(text=f"Saved to slot {slot}.")]
        if cmd == "load":
            slot = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 1
            try:
                self.load(slot)
            except FileNotFoundError:
                return [GameEvent(text=f"No save in slot {slot}.")]
            return [GameEvent(text=f"Loaded slot {slot}."), *self._look()]
        return [GameEvent(text=f"Unknown command: {text}")]

    def _look(self) -> list[GameEvent]:
        loc_id = self.world.player.current_location
        loc = self.world.locations[loc_id]
        lines = [loc.name, loc.base_description]
        items = [
            self.world.items[iid].name
            for iid, owner in self.world.item_owners.items()
            if owner == "location"
            and self.world.item_locations.get(iid) == loc_id
            and iid in self.world.items
            and not self.world.items[iid].hidden
        ]
        if items:
            lines.append("You see: " + ", ".join(items))
        npcs = [
            n.identity.name for n in self.world.npcs.values() if n.state.current_location == loc_id
        ]
        if npcs:
            lines.append("Here: " + ", ".join(npcs))
        exits = []
        locked = loc.locked_directions()
        for direction, _dest in loc.exits.items():
            label = f"{direction} (locked)" if direction in locked else direction
            exits.append(label)
        if exits:
            lines.append("Exits: " + ", ".join(exits))
        lines.append(f"[{self.world.clock.period_name}]")
        return [GameEvent(kind="look", text="\n".join(lines))]

    def _inventory(self) -> list[GameEvent]:
        if not self.world.player.inventory:
            return [GameEvent(text="You are carrying nothing.")]
        names = [
            self.world.items[i].name for i in self.world.player.inventory if i in self.world.items
        ]
        return [GameEvent(text="You are carrying: " + ", ".join(names))]

    def _resolve_item_ref(self, text: str) -> str | None:
        needle = text.strip().lower()
        if not needle:
            return None
        if needle in self.world.items:
            return needle
        for iid, item in self.world.items.items():
            if item.name.lower() == needle or needle in item.name.lower():
                return iid
        return None

    def _resolve_npc_ref(self, text: str) -> str | None:
        needle = text.strip().lower()
        if not needle:
            return None
        if needle in self.world.npcs:
            return needle
        for nid, npc in self.world.npcs.items():
            if npc.identity.name.lower() == needle or needle in npc.identity.name.lower():
                return nid
        return None

    def _go(self, direction: str) -> list[GameEvent]:
        direction = direction.strip().lower()
        if not direction:
            return [GameEvent(text="Go where?")]
        loc = self.world.locations[self.world.player.current_location]
        if direction not in loc.exits:
            return [GameEvent(text="You can't go that way.")]
        if direction in loc.locked_directions():
            return [GameEvent(text="That way is locked.")]
        self.world.player.current_location = loc.exits[direction]
        self._significant = True
        return self._look()

    def _examine(self, arg: str) -> list[GameEvent]:
        item_id = self._resolve_item_ref(arg)
        if not item_id:
            return [GameEvent(text="You don't see that here.")]
        if not self._item_accessible(item_id):
            return [GameEvent(text="You don't see that here.")]
        item = self.world.items[item_id]
        text = item.description
        if item.properties.get("readable") == "true" and item.properties.get("text"):
            text = f"{text}\nIt reads: {item.properties['text']}"
        return [GameEvent(text=text)]

    def _item_accessible(self, item_id: str) -> bool:
        owner = self.world.item_owners.get(item_id)
        if owner == "player" or item_id in self.world.player.inventory:
            return True
        here = self.world.player.current_location
        if owner == "location" and self.world.item_locations.get(item_id) == here:
            return True
        return False

    def _take(self, arg: str) -> list[GameEvent]:
        item_id = self._resolve_item_ref(arg)
        if not item_id:
            return [GameEvent(text="Take what?")]
        item = self.world.items[item_id]
        loc_id = self.world.player.current_location
        owner = self.world.item_owners.get(item_id)
        if owner != "location" or self.world.item_locations.get(item_id) != loc_id:
            return [GameEvent(text="You don't see that here.")]
        if not item.takeable:
            return [GameEvent(text="You can't take that.")]
        self.world.item_owners[item_id] = "player"
        self.world.item_locations[item_id] = loc_id
        self.world.player.inventory.append(item_id)
        loc = self.world.locations[loc_id]
        if item_id in loc.items:
            loc.items.remove(item_id)
        self._significant = True
        return [GameEvent(text=f"You take the {item.name}.")]

    def _drop(self, arg: str) -> list[GameEvent]:
        item_id = self._resolve_item_ref(arg)
        if not item_id or item_id not in self.world.player.inventory:
            return [GameEvent(text="You aren't carrying that.")]
        item = self.world.items[item_id]
        loc_id = self.world.player.current_location
        self.world.player.inventory.remove(item_id)
        self.world.item_owners[item_id] = "location"
        self.world.item_locations[item_id] = loc_id
        self.world.locations[loc_id].items.append(item_id)
        self._significant = True
        return [GameEvent(text=f"You drop the {item.name}.")]

    def _use(self, arg: str) -> list[GameEvent]:
        match = re.match(r"(.+?)\s+on(?:/with)?\s+(.+)", arg, flags=re.I)
        if not match:
            # also accept "use X with Y"
            match = re.match(r"(.+?)\s+with\s+(.+)", arg, flags=re.I)
        if not match:
            return [GameEvent(text="Use what on what?")]
        item_id = self._resolve_item_ref(match.group(1))
        target = match.group(2).strip().lower()
        if not item_id or item_id not in self.world.player.inventory:
            return [GameEvent(text="You aren't carrying that.")]
        item = self.world.items[item_id]
        loc = self.world.locations[self.world.player.current_location]
        unlock = item.unlock_target.strip().lower()
        # unlock_target may be a direction or location id
        matches = target == unlock or (target in loc.exits and loc.exits[target] == unlock)
        if unlock and matches:
            # unlock matching locked exit
            unlocked_any = False
            new_locked = []
            for entry in loc.locked_exits:
                direction = entry if isinstance(entry, str) else entry.get("direction")
                if direction and (
                    direction == target or loc.exits.get(direction) == unlock or direction == unlock
                ):
                    unlocked_any = True
                    continue
                new_locked.append(entry)
            locked_now = loc.locked_directions()
            if unlocked_any or target in locked_now or unlock in locked_now:
                # rebuild locked_exits without the unlocked direction
                direction_to_clear = target if target in loc.exits else unlock
                loc.locked_exits = [
                    e
                    for e in loc.locked_exits
                    if (e if isinstance(e, str) else e.get("direction")) != direction_to_clear
                ]
                self._significant = True
                return [GameEvent(text=f"You use the {item.name}. The way is unlocked.")]
        return [GameEvent(text="That doesn't work.")]

    def _talk(self, arg: str) -> list[GameEvent]:
        npc_id = self._resolve_npc_ref(arg)
        if not npc_id:
            return [GameEvent(text="Talk to whom?")]
        npc = self.world.npcs[npc_id]
        if npc.state.current_location != self.world.player.current_location:
            return [GameEvent(text=f"{npc.identity.name} isn't here.")]
        self._phase = GamePhase.IN_CONVERSATION
        self._active_npc = npc_id
        npc.state.has_met_player = True
        return [GameEvent(text=f"You begin talking with {npc.identity.name}.")]

    # --- NPC tools -------------------------------------------------------

    def _validate_npc_tool(self, action: Action) -> tuple[bool, str]:
        npc_id = action.actor_id
        if npc_id not in self.world.npcs:
            return False, "Unknown NPC"
        npc = self.world.npcs[npc_id]
        policy = npc.identity.tool_policy
        if action.type not in policy.allowed_tools:
            return False, f"Tool '{action.type}' not allowed for {npc.identity.name}"

        params = action.params
        if action.type in {"give_item", "take_item", "inspect_item"}:
            item_id = str(params.get("item_id", ""))
            if policy.allowed_items and item_id not in policy.allowed_items:
                return False, f"Item '{item_id}' not allowed"
            if item_id not in self.world.items:
                return False, f"Unknown item '{item_id}'"
            if action.type == "give_item" and item_id not in npc.state.inventory:
                return False, "NPC does not hold that item"
            if action.type == "take_item" and item_id not in self.world.player.inventory:
                return False, "Player does not hold that item"
        if action.type == "update_mood":
            mood = str(params.get("mood", ""))
            if mood not in VALID_MOODS:
                return False, f"Invalid mood '{mood}'"
        if action.type == "update_trust":
            try:
                int(params.get("delta"))
            except (TypeError, ValueError):
                return False, "Trust delta must be an integer"
        if action.type == "move_self":
            loc = str(params.get("location_id", ""))
            if loc not in self.world.locations:
                return False, "Unknown location"
            if policy.allowed_locations and loc not in policy.allowed_locations:
                return False, "Location not allowed"
        if action.type == "reveal_knowledge":
            fact_id = str(params.get("fact_id", ""))
            if fact_id not in npc.identity.knowledge:
                return False, "NPC does not know that fact"
            if policy.allowed_facts and fact_id not in policy.allowed_facts:
                return False, "Fact not allowed"
            if fact_id not in self.world.facts:
                return False, "Unknown fact"
        if action.type == "set_flag":
            flag_id = str(params.get("flag_id", ""))
            if policy.allowed_flags and flag_id not in policy.allowed_flags:
                return False, "Flag not allowed"
            if flag_id not in self.world.flags and flag_id not in self.world.flag_meta:
                return False, "Unknown flag"
        if action.type == "remember":
            if not str(params.get("summary", "")).strip():
                return False, "Memory summary required"
        return True, ""

    def _apply_one(self, action: Action) -> list[GameEvent]:
        t = action.type
        if t == "say":
            npc = self.world.npcs[action.actor_id]
            text = str(action.params.get("text", "")).strip()
            return [GameEvent(kind="dialogue", text=f'{npc.identity.name}: "{text}"')]
        if t == "give_item":
            return self._npc_give(action)
        if t == "take_item":
            return self._npc_take(action)
        if t == "update_mood":
            return self._npc_mood(action)
        if t == "update_trust":
            return self._npc_trust(action)
        if t == "move_self":
            return self._npc_move(action)
        if t == "reveal_knowledge":
            return self._npc_reveal(action)
        if t == "remember":
            return self._npc_remember(action)
        if t == "set_flag":
            return self._npc_set_flag(action)
        if t == "inspect_item":
            return self._npc_inspect(action)
        if t == "event_narrate":
            return [GameEvent(kind="narration", text=str(action.params.get("text", "")))]
        if t == "event_end_game":
            self._phase = GamePhase.GAME_OVER
            self._active_npc = None
            text = str(action.params.get("text", "The scenario ends."))
            return [GameEvent(kind="ending", text=text)]
        if t == "event_move_npc":
            return self._event_move_npc(action)
        if t == "event_set_flag":
            return self._event_set_flag(action)
        if t == "event_spawn_item":
            return self._event_spawn_item(action)
        return []

    def _narrate(self, key: str, **kwargs: str) -> GameEvent | None:
        template = self.world.config.mutation_narration_templates.get(key, "")
        if not template:
            return None
        return GameEvent(kind="narration", text=template.format(**kwargs))

    def _npc_give(self, action: Action) -> list[GameEvent]:
        npc = self.world.npcs[action.actor_id]
        item_id = str(action.params["item_id"])
        item = self.world.items[item_id]
        npc.state.inventory.remove(item_id)
        self.world.player.inventory.append(item_id)
        self.world.item_owners[item_id] = "player"
        self.world.item_locations[item_id] = self.world.player.current_location
        self._significant = True
        ev = self._narrate("give_item_to_player", npc=npc.identity.name, item=item.name)
        return [ev] if ev else [GameEvent(text=f"{npc.identity.name} gives you the {item.name}.")]

    def _npc_take(self, action: Action) -> list[GameEvent]:
        npc = self.world.npcs[action.actor_id]
        item_id = str(action.params["item_id"])
        item = self.world.items[item_id]
        self.world.player.inventory.remove(item_id)
        npc.state.inventory.append(item_id)
        self.world.item_owners[item_id] = action.actor_id
        self.world.item_locations[item_id] = npc.state.current_location
        self._significant = True
        ev = self._narrate("take_item_from_player", npc=npc.identity.name, item=item.name)
        return [ev] if ev else [GameEvent(text=f"{npc.identity.name} takes the {item.name}.")]

    def _npc_mood(self, action: Action) -> list[GameEvent]:
        npc = self.world.npcs[action.actor_id]
        mood = str(action.params["mood"])
        npc.state.mood = mood
        ev = self._narrate("update_npc_mood", npc=npc.identity.name, mood=mood)
        return [ev] if ev else []

    def _npc_trust(self, action: Action) -> list[GameEvent]:
        npc = self.world.npcs[action.actor_id]
        delta = int(action.params["delta"])
        npc.state.trust_toward_player = max(0, min(100, npc.state.trust_toward_player + delta))
        if (
            npc.identity.secret
            and npc.state.trust_toward_player >= npc.identity.trust_reveal_threshold
        ):
            npc.state.secret_revealed = True
        ev = self._narrate("update_npc_trust", npc=npc.identity.name)
        return [ev] if ev else []

    def _npc_move(self, action: Action) -> list[GameEvent]:
        npc = self.world.npcs[action.actor_id]
        loc = str(action.params["location_id"])
        npc.state.current_location = loc
        self._significant = True
        if self._active_npc == action.actor_id and loc != self.world.player.current_location:
            self._phase = GamePhase.PLAYING
            self._active_npc = None
        ev = self._narrate(
            "move_npc",
            npc=npc.identity.name,
            location=self.world.locations[loc].name,
        )
        return [ev] if ev else []

    def _npc_reveal(self, action: Action) -> list[GameEvent]:
        fact_id = str(action.params["fact_id"])
        fact = self.world.facts[fact_id]
        self.world.revealed_facts.add(fact_id)
        self._significant = True
        ev = self._narrate("reveal_knowledge")
        events = [ev] if ev else []
        events.append(GameEvent(kind="knowledge", text=fact.text))
        return events

    def _npc_remember(self, action: Action) -> list[GameEvent]:
        npc = self.world.npcs[action.actor_id]
        summary = str(action.params["summary"]).strip()
        importance = int(action.params.get("importance", 5))
        npc.state.memories.append(
            MemoryEntry(
                timestamp=self.world.clock.period_name,
                summary=summary,
                importance=importance,
            )
        )
        ev = self._narrate("add_memory")
        return [ev] if ev else []

    def _npc_set_flag(self, action: Action) -> list[GameEvent]:
        flag_id = str(action.params["flag_id"])
        value = action.params.get("value", True)
        if isinstance(value, str):
            value = value.lower() in {"1", "true", "yes"}
        self.world.flags[flag_id] = bool(value)
        self._significant = True
        ev = self._narrate("set_flag")
        return [ev] if ev else []

    def _npc_inspect(self, action: Action) -> list[GameEvent]:
        item_id = str(action.params["item_id"])
        item = self.world.items[item_id]
        return [GameEvent(kind="tool_result", text=f"[inspect] {item.name}: {item.description}")]

    # --- events ----------------------------------------------------------

    def _evaluate_events(self) -> list[GameEvent]:
        events: list[GameEvent] = []
        actions: list[Action] = []
        for _event_id, trigger in self.world.events.items():
            if trigger.once and trigger.fired:
                continue
            if not all(self._condition_met(c.type, c.args) for c in trigger.conditions):
                continue
            trigger.fired = True
            for act in trigger.actions:
                actions.append(self._event_action_to_action(act.type, act.params))
        if actions:
            # Event actions are trusted after structural validation; still apply via apply().
            for action in actions:
                ok, _ = self.validate_action(action)
                if ok or action.source == ActionSource.EVENT:
                    events.extend(self._apply_one(action))
        return events

    def _condition_met(self, ctype: str, args: list[str]) -> bool:
        if ctype == "clock_is":
            return self.world.clock.period_name == args[0]
        if ctype == "player_at":
            return self.world.player.current_location == args[0]
        if ctype == "flag_set":
            want = args[1].lower() in {"1", "true", "yes"}
            return bool(self.world.flags.get(args[0], False)) is want
        if ctype == "npc_trust_ge":
            npc = self.world.npcs.get(args[0])
            return bool(npc and npc.state.trust_toward_player >= int(args[1]))
        if ctype == "npc_at":
            npc = self.world.npcs.get(args[0])
            return bool(npc and npc.state.current_location == args[1])
        if ctype == "item_in_player_inv":
            return args[0] in self.world.player.inventory
        if ctype == "turn_ge":
            return self.world.clock.turns_elapsed >= int(args[0])
        return False

    def _event_action_to_action(self, atype: str, params: dict) -> Action:
        mapping = {
            "narrate": "event_narrate",
            "end_game": "event_end_game",
            "move_npc": "event_move_npc",
            "set_flag": "event_set_flag",
            "spawn_item": "event_spawn_item",
        }
        return Action(
            type=mapping.get(atype, atype),
            source=ActionSource.EVENT,
            params=dict(params),
        )

    def _event_move_npc(self, action: Action) -> list[GameEvent]:
        npc_id = str(action.params["npc_id"])
        loc = str(action.params["location_id"])
        self.world.npcs[npc_id].state.current_location = loc
        name = self.world.npcs[npc_id].identity.name
        return [GameEvent(text=f"{name} moves to {self.world.locations[loc].name}.")]

    def _event_set_flag(self, action: Action) -> list[GameEvent]:
        flag_id = str(action.params["flag_id"])
        value = action.params.get("value", True)
        if isinstance(value, str):
            value = value.lower() in {"1", "true", "yes"}
        self.world.flags[flag_id] = bool(value)
        return []

    def _event_spawn_item(self, action: Action) -> list[GameEvent]:
        item_id = str(action.params["item_id"])
        loc = str(action.params["location_id"])
        self.world.item_owners[item_id] = "location"
        self.world.item_locations[item_id] = loc
        if item_id not in self.world.locations[loc].items:
            self.world.locations[loc].items.append(item_id)
        return [GameEvent(text=f"Something new appears in {self.world.locations[loc].name}.")]
