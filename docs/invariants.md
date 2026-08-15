# Chronicle Invariants

These rules are non-negotiable for the console runtime.

1. **LLM proposes; console decides.** Model output never writes world state directly.
2. **Single action gate.** Player commands, NPC tools, and scripted events all become
   validated actions applied in one place.
3. **Deterministic durable state.** World is typed, serializable data. Emergence comes
   only through validated tools.
4. **Authored knowledge only.** Facts an NPC may reveal are declared in the cartridge;
   the model cannot invent lore into state.
5. **Graceful degradation.** Missing or failing model yields stub dialogue; exploration,
   events, and save/load still work.
6. **Cartridges are untrusted data.** Path-safe load, schema version checks, and
   cross-reference validation happen before play.
7. **Public contract = CLI + cartridge schema.** Internal modules may change
   without compatibility guarantees.
