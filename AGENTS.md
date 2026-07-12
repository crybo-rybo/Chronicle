# Chronicle

Chronicle is a bounded offline console for LLM-driven NPC mystery and social-sim
text adventures written in Python. Creators author JSON scenario cartridges;
Chronicle supplies the runtime, local OpenAI-compatible LLM integration,
validation, prompt assembly, save/load, and a strict tool/action gate.

When starting a new session, read `README.md` first, then `docs/invariants.md`,
`docs/console-api.md`, and `docs/scenario-package-schema.md`.

## Invariants

1. LLM proposes; console decides.
2. Single action gate for all world writes.
3. Deterministic durable state.
4. Authored knowledge only.
5. Graceful degradation without a model.
6. Cartridges are untrusted data.
7. Public contract = CLI + cartridge schema (+ optional GameBackend protocol).

## Layout

- `src/chronicle/` — runtime package
- `examples/` — sample cartridges
- `docs/` — creator and design docs
- `tests/` — unit tests (no model required)

## Session cleanup

Remove any files generated under `docs/superpowers/` during the session.
