# Chronicle 1.0 AI Agent Game SDK Integration Plan

## Summary

Chronicle 1.0 will complete the transition from a single authored game into a
bounded offline AI agent game SDK/runtime for NPC mystery and social-sim text
adventures.

The stable 1.0 SDK surface is deliberately narrow:

- CLI runner: `chronicle [--scenario <dir>]`
- CLI validator: `chronicle validate --scenario <dir>`
- JSON scenario package schema and validation behavior

C++ headers, library boundaries, and internal class APIs remain implementation
details for 1.0.

Current observed baseline:

- Default non-model test suite passes: 316/316 tests.
- `data/` validates successfully.
- `examples/lighthouse_veil/` validates successfully.
- Existing implementation already includes scenario manifests, JSON schemas,
  scenario authoring docs, tool policy, prompt building, save/load, CLI
  validation, and model-gated integration test structure.

## 1. Lock The 1.0 SDK Contract

### Implementation Details

- Update release-facing docs so they consistently describe Chronicle as an
  offline AI agent game SDK/runtime, not a single fixed game.
- Keep `docs/chronicle-scenario-sdk-pivot.md` as the canonical short statement
  of the 1.0 public contract.
- Update `README.md` to clearly separate stable 1.0 contract from internal C++
  implementation details.
- Update `docs/scenario-authoring-guide.md` to remove stale language such as
  schemas or examples "landing in parallel" when they already exist.
- Ensure external design documents referenced through `.secret/local_paths.md`
  remain conceptually aligned with the repo docs.
- Do not introduce a stable embeddable C++ API before 1.0.

### Exit Criteria

- README, pivot doc, authoring guide, schemas, and design-doc language all agree
  that the stable 1.0 surface is CLI plus JSON scenario packages.
- No public documentation implies that C++ headers or libraries are stable APIs.
- A new reader understands that `data/` is a sample package, not the product
  itself.

## 2. Add A Formal Scenario Package Schema Reference

### Implementation Details

- Add `docs/scenario-package-schema.md` as the concise field-by-field reference
  for the 1.0 JSON contract.
- Cover all author-facing package files:
  - `scenario.json`
  - `config.json`
  - `world.json`
  - `npcs.json`
  - `facts.json`
  - `flags.json`
  - `events.json`
- For each file, document:
  - required fields
  - optional/defaulted fields
  - ID and cross-reference rules
  - runtime behavior
  - validation behavior
  - one minimal valid example
- Document map-key ID injection explicitly: entity IDs come from JSON object
  keys and are injected by the loader.
- Document path safety rules for manifest file paths.
- Document NPC tool policy semantics:
  - missing `tool_policy` defaults to the full built-in palette
  - empty `allowed_tools` means no tool permissions
  - empty scoped ID lists mean no additional restriction
- Link the schema reference from `README.md` and the authoring guide.

### Exit Criteria

- A new author can create a two-room, one-NPC package from docs alone.
- Every documented minimal example can be represented by a valid fixture package.
- The schema reference agrees with `schemas/*.schema.json` and current C++ loader
  behavior.

## 3. Harden Validator Diagnostics

### Implementation Details

- Improve `validate_scenario_package` and `validate_world` diagnostics with
  file, entity, and field context where practical.
- Include offending IDs in all cross-reference failures.
- Include expected/current schema versions for unsupported
  `chronicle_schema_version`.
- Validate manifest identity fields:
  - non-empty `id`
  - non-empty `name`
  - non-empty `version`
- Validate `metadata` remains string-to-string and report useful errors for
  invalid shapes.
- Preserve warning-level diagnostics for suspicious but valid authoring data,
  such as `readable=true` items without text.
- Keep validator model-free and suitable for CI.
- Prefer deterministic, stable diagnostic fragments so tests can assert useful
  messages without depending on entire multiline outputs.

### Exit Criteria

- `chronicle validate --scenario <bad-fixture>` returns actionable errors for:
  - missing scenario directory
  - missing manifest
  - malformed manifest JSON
  - unsupported schema version
  - absolute manifest path
  - path traversal
  - missing referenced file
  - malformed world/NPC/fact/flag/event JSON
  - missing location reference
  - duplicate item ownership
  - unknown NPC tool
  - missing policy item/fact/flag/location
  - invalid event condition argument count
  - invalid event action type
- Warnings are printed without failing otherwise valid packages.

## 4. Add Scenario Compatibility Fixture Packages

### Implementation Details

- Create `tests/fixtures/scenarios/`.
- Add one valid minimal package that mirrors the schema reference examples.
- Add targeted invalid packages for each major validation class.
- Add tests that call validation on fixture directories and assert stable
  diagnostic fragments.
- Add tests for public compatibility semantics:
  - defaulted manifest file names
  - optional metadata
  - missing `tool_policy` default behavior
  - empty scoped policy lists meaning unrestricted scope
  - empty `allowed_tools` meaning no tool permissions
- Keep the existing direct entity/unit tests; fixture packages are contract
  tests, not replacements.
- Keep CI validation for `data/`.
- Add CI validation for `examples/lighthouse_veil/` if that example remains
  shipped as a first-class reference package.

### Exit Criteria

- Schema drift breaks tests before release.
- Fixture coverage exists for every validator diagnostic category listed in
  this plan.
- CI validates shipped scenario packages without requiring a model.

## 5. Implement The Post-Turn Pipeline

### Implementation Details

- Refactor `GameEngine` command handling so successful significant actions
  advance time exactly once.
- Significant actions for 1.0:
  - successful `go`
  - successful `take`
  - successful `drop`
  - successful `use`
  - completed successful dialogue turn
- Non-advancing actions:
  - `look`
  - `examine`
  - `inventory`
  - `help`
  - `save`
  - `load`
  - failed commands
  - entering or leaving a conversation
  - `quit`
- After each advancing action:
  1. apply pending mutations
  2. advance `world_.clock`
  3. render time-period transition if the visible period changed
  4. evaluate scripted events
  5. apply event mutations
  6. narrate event results
- Avoid nested or duplicate `process_pending_mutations()` calls causing a single
  player command to apply more than one turn of time.
- Keep save/load behavior deterministic: loading resets active conversation and
  transient queues but preserves durable clock/world state.

### Exit Criteria

- Clock advances deterministically for every significant action.
- Non-significant commands do not change time.
- Period changes render once.
- Tests cover advancing and non-advancing commands.
- No command double-applies mutations or advances time twice.

## 6. Execute Scripted Events

### Implementation Details

- Add event condition evaluation for every existing `ConditionType`:
  - `clock_is`
  - `player_at`
  - `flag_set`
  - `npc_trust_ge`
  - `npc_at`
  - `item_in_player_inv`
  - `turn_ge`
- Preserve AND semantics: all conditions in a trigger must be true.
- Skip fired one-shot events.
- Execute actions:
  - `move_npc`: create `MutationRequest::MoveNpc` with `Source::System`
  - `set_flag`: create `MutationRequest::SetFlag` with `Source::System`
  - `spawn_item`: create `MutationRequest::SpawnItem` with `Source::System`
  - `narrate`: render the text directly through the renderer
  - `end_game`: enter the resolution path
- Mark `once=true` events as fired after successful trigger handling.
- Decide and document behavior for `once=false`: it may fire every eligible
  post-turn pass and should not set `fired=true`.
- Ensure event mutation application still goes through the single mutation
  application path owned by `GameEngine`.
- Ensure event `fired` state persists through save/load using existing world
  serialization.

### Exit Criteria

- Each condition type has true and false tests.
- Event actions apply expected state changes through the mutation pipeline.
- Narration actions render in deterministic order.
- One-shot events do not refire after save/load.
- Validate mode loads and checks events but never executes them.

## 7. Finish Event-Only Resolution

### Implementation Details

- Standardize event-only endings for 1.0; do not add `endings.json`.
- Extend `end_game` handling so it may use optional `params.text` for final
  narration.
- If `params.text` is missing, render a generic ending message such as:
  `The scenario has reached its conclusion.`
- Transition phases predictably:
  - event fires `end_game`
  - renderer displays resolution text
  - `phase_` becomes `GameOver`
- Use `Resolution` only if implementation needs a short internal intermediate
  state; do not leave the player stuck there.
- Restrict `GameOver` commands to:
  - `help`
  - `load`
  - `quit`
- Return clear errors for normal gameplay commands after game over.
- Add at least two deterministic ending states across the bundled and example
  packages using flags/facts/time/event conditions.

### Exit Criteria

- A scenario can end without hardcoded game-specific logic.
- At least one shipped package reaches a complete ending through `events.json`.
- GameOver phase rejects normal play commands and permits load/quit/help.
- No new scenario file is added for endings before 1.0.

## 8. Implement `use` And Locked Exits

### Implementation Details

- Keep item interactions in `world.json` for 1.0 using existing fields:
  - `Location.locked_exits`
  - `Item.unlock_target`
- Validate every `locked_exits` entry references a key in the same location's
  `exits` map.
- Update movement validation so locked exits block traversal with a clear
  message.
- Add internal `MutationRequest::UnlockExit`.
- Add mutation application that removes a direction from a location's
  `locked_exits`.
- Implement `use <item> on <target>` and `use <item> with <target>` in
  `GameEngine`.
- Match the item from player inventory by ID or display name.
- Match the target against:
  - locked direction name
  - destination location ID
  - destination location display name
- Unlock only when the inventory item's `unlock_target` matches the locked
  exit destination location ID.
- Render a concise success message and keep successful use as a significant
  time-advancing action.
- Reject invalid use attempts gracefully:
  - item not held
  - no target provided
  - target not locked
  - item does not unlock that target
- Ensure unlocked state persists because `Location.locked_exits` is already
  part of world serialization.

### Exit Criteria

- Locked exits block movement.
- Correct item use unlocks the intended exit.
- Incorrect item/target use fails without mutating state.
- Unlock state survives save/load.
- `examples/lighthouse_veil` tide-gate flow can be completed through authored
  data and `use`, not C++ game-specific logic.

## 9. Add Prompt Contract Snapshot Tests

### Implementation Details

- Add golden prompt tests for:
  - static system prompt
  - dynamic context
  - user turn
- Cover the sample NPCs from `data/` or a stable prompt fixture world.
- Assert:
  - identity/personality/backstory/goals appear in static prompt
  - dynamic mood/trust/location/time appear in dynamic context
  - secrets appear only when trust threshold is met
  - facts resolve through `world.facts`
  - player input is JSON-encoded
  - inventory context is included only when relevant
  - dynamic context does not pollute the user turn
  - static prompt excludes volatile world state
- Keep snapshots readable and reviewable; avoid brittle ANSI/output formatting.

### Exit Criteria

- Prompt refactors produce meaningful test diffs.
- Secret gating is protected by tests.
- User input escaping is protected by tests.
- Prompt tests do not require a model.

## 10. Harden Explicit NPC Memory

### Implementation Details

- Keep explicit `remember` as the only 1.0 memory surface.
- Do not add automatic post-conversation memory extraction before 1.0.
- Improve the `remember` tool description and prompt rules so models understand
  when to create durable memories.
- Keep memory creation flowing through `MutationRequest::AddMemory`.
- Ensure memory narration remains silent unless the scenario explicitly
  narrates a separate event.
- Add tests that:
  - a valid `remember` tool call persists a memory
  - empty summaries are rejected
  - importance is clamped
  - saved and loaded worlds preserve NPC memories
  - re-entering a conversation includes relevant prior memories in dynamic
    context

### Exit Criteria

- NPC memory survives save/load.
- An NPC can refer to prior interactions after leaving and re-entering a
  conversation, assuming the model uses the supplied context.
- No automatic extractor is required for 1.0.

## 11. Add Inference Timeout And Failure Recovery

### Implementation Details

- Add optional `Config::inference_timeout_ms`.
- Default to `120000` milliseconds.
- Treat `0` as disabling timeout/cancellation for local debugging.
- Add the field to:
  - C++ config struct and JSON serialization
  - `schemas/config.schema.json`
  - `data/config.json`
  - examples/templates as appropriate
  - docs
- Thread the timeout into `ZooAgentAdapter`.
- Use Zoo-Keeper `RequestHandle::cancel()` when timeout elapses.
- Convert timeout/cancel into `AgentChatResult{false, ...}` with a useful error.
- On chat failure, timeout, cancellation, or exception:
  - do not apply NPC pending mutations from that failed turn
  - clear pending tool mutations
  - render a player-facing error/fallback message
  - keep movement, inventory, save, load, help, and quit available
- Keep existing tool-loop nudge behavior, but ensure repeated tool-loop failures
  do not expose raw backend errors unnecessarily.
- Avoid logging hidden scenario content unless log level and documentation make
  that tradeoff clear.

### Exit Criteria

- Mock tests cover chat failure, thrown exceptions, empty response, malformed
  tool args, timeout/cancel, and all-invalid tool calls.
- Failed dialogue does not mutate world state.
- The player remains able to leave conversation or continue deterministic
  gameplay after model failure.
- Timeout behavior is documented.

## 12. Polish CLI And Runtime UX

### Implementation Details

- Implement `help` handling in:
  - `Playing`
  - `InConversation`
  - `GameOver`
- Include concise command lists without exposing internal implementation.
- Improve first-run empty-model behavior:
  - clearly state that AI dialogue is using stub output because no local model
    is configured
  - point to docs for local model configuration
- Keep empty `model_path` as the 1.0 no-AI/stub path.
- Do not add `--no-ai` for 1.0 unless needed later; it is redundant with empty
  `model_path`.
- Improve invalid command messages for unsupported phase-specific actions.
- Document logging:
  - `CHRONICLE_LOG_FILE`
  - `CHRONICLE_LOG_LEVEL`
  - `CHRONICLE_LOG=off`
  - safe sharing expectations for logs
- Ensure `use_color` is respected by the terminal renderer for all newly added
  messages.

### Exit Criteria

- A new user can build, validate, and run the sample without asking how to
  start.
- In-game help exposes all 1.0 player commands.
- Stub dialogue mode is understandable and not mistaken for a crash.
- Runtime UX tests cover help and phase-specific command restrictions.

## 13. Update Sample Package And Creator Template

### Implementation Details

- Keep `data/` compact: small enough to inspect quickly, but broad enough to
  demonstrate the 1.0 contract.
- Ensure `data/` exercises:
  - scenario manifest
  - config
  - world graph
  - NPC identity/state
  - facts
  - flags
  - events
  - tool policy restriction
  - readable item inspection
  - explicit memory
  - save/load relevant state
  - at least one ending
- If `data/` remains intentionally tiny, use `examples/lighthouse_veil/` for
  the richer locked-exit, multi-NPC, and event-chain demonstration.
- Add `examples/minimal_scenario/` as a copyable starter package.
- Keep JSON free of comments; put explanations in docs.
- Update authoring guide walkthroughs to match the actual shipped files.

### Exit Criteria

- `data/` validates and demonstrates the core loop.
- `examples/lighthouse_veil/` validates and demonstrates richer scenario
  mechanics.
- `examples/minimal_scenario/` validates and is small enough to copy.
- README and authoring guide examples match real repo files.

## 14. Release Hardening

### Implementation Details

- Add an MIT `LICENSE` file.
- Add `CHANGELOG.md` with an initial `v1.0.0` section.
- Add a release checklist covering:
  - formatting
  - build
  - tests
  - scenario validation
  - optional model-gated integration run
  - sample playthrough
  - docs review
- Run clang-format or `scripts/format.sh` before release.
- Run warning-as-error build:
  - `cmake -B build -DCMAKE_BUILD_TYPE=Release -DCHRONICLE_WERROR=ON`
  - `cmake --build build --parallel`
- Run default tests:
  - `ctest --test-dir build --output-on-failure`
- Run validation:
  - `./build/src/chronicle validate --scenario data`
  - `./build/src/chronicle validate --scenario examples/lighthouse_veil`
  - validation for any new template/example packages
- Run one representative AddressSanitizer build/session where practical.
- Keep model-backed tests opt-in through `ZOO_INTEGRATION_MODEL`.
- Clean up any generated files under `docs/superpowers/` before ending each
  development session.

### Exit Criteria

- CI is green on Linux and macOS.
- Local warning-as-error build passes.
- All default non-model tests pass.
- Shipped scenarios validate.
- MIT license and changelog are present.
- No temporary planning files remain under `docs/superpowers/`.
- Repository is ready to tag `v1.0.0`.

## Document-Wide / 1.0-Wide Exit Criteria

Chronicle 1.0 is complete when all of the following are true:

- Chronicle is clearly presented as an offline AI agent game SDK/runtime for
  bounded mystery/social-sim scenario packages.
- The public contract is stable and documented: CLI plus JSON scenario package
  schema only.
- A creator can copy a minimal package, validate it, run it, and fix common
  errors from CLI diagnostics.
- The bundled sample demonstrates deterministic world state, constrained NPC
  tool use, readable item inspection, scripted events, explicit memory,
  save/load, and a complete ending.
- The richer example package demonstrates multi-location authoring, locked
  exits, authored item use, event chains, and policy-scoped NPC tools.
- Model loading failure, inference failure, timeout, invalid tool calls, and
  missing local model configuration do not corrupt world state or strand the
  player.
- All state changes from players, NPC tools, and system events continue to flow
  through the single mutation gate.
- All default non-model tests pass locally and in CI.
- Model-backed tests remain opt-in and documented.
- README, authoring guide, schema reference, JSON schemas, examples, license,
  changelog, and release notes are aligned for `v1.0.0`.

## Assumptions And Defaults

- Stable 1.0 SDK surface is CLI plus JSON scenario packages.
- C++ APIs remain internal implementation details.
- Scenario schema version remains `1`.
- Resolution is event-only for 1.0; no `endings.json`.
- Memory is explicit through the `remember` tool; no automatic extraction before
  1.0.
- Empty `model_path` remains the no-AI/stub-dialogue mode.
- 1.0 license is MIT.
