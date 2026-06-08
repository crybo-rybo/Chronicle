# Chronicle

Chronicle is a bounded scenario SDK/runtime for offline, LLM-driven NPC mystery and social-sim text adventures written in C++23. Creators author JSON scenario packages; Chronicle supplies the runtime, local zoo-keeper-harness integration for OpenAI-compatible endpoints, validation, prompt assembly, save/load, and strict tool/mutation pipeline.

## Vision

A portfolio piece and open-source framework demonstrating systems-level C++ design, applied LLM integration, deterministic state management, and a small but useful authoring surface for AI-driven text adventure scenarios.

## Architecture Overview

- **Engine layer** (`src/engine/`): CLI parser, GameEngine orchestrator, command parser, mutation pipeline, scripted events
- **World layer** (`chronicle_world`, `src/entities/` structs): NPC, Player, Item, Location runtime state and serialization
- **Scenario layer** (`chronicle_scenario`): Cartridge manifest, config, world loader, structural validation
- **AI layer** (`src/ai/`): zoo-keeper-harness agent wrapper, prompt builder, tool registry, response handler, explicit NPC memory via `remember`
- **Rendering layer** (`src/rendering/`): Abstract renderer interface, terminal renderer (MVP), TUI renderer (stretch goal)
- **Persistence layer** (`src/persistence/`): Save/load system with versioned schemas
- **Scenario package layer** (`data/`, `scenario.json`): Author-facing JSON package format and validation

### Key Design Invariants

1. **Single mutation gate**: Only GameEngine writes to World. All state changes flow through validated MutationRequest pipeline.
2. **AI layer reads-only**: AI reads World to build prompts but never directly mutates it.
3. **Deterministic state, emergent behavior**: World is typed C++ structs; LLM influences only through validated tool calls.
4. **Graceful degradation**: If inference fails/hangs, game remains playable; NPCs fall back to idle behavior.
5. **Bounded framework surface**: V1 public API is CLI + scenario schema, not stable C++ APIs.

## Tech Stack

- **Language**: C++23
- **Build**: CMake 3.25+
- **LLM Integration**: zoo-keeper-harness (via CMake FetchContent)
- **JSON**: nlohmann/json (fetched directly and shared with the harness)
- **Testing**: Google Test (FetchContent)
- **CI**: GitHub Actions (Linux + macOS)

## Design Documents

Full design documents are stored outside this repository. See `.secret/local_paths.md` for their location on your local machine. The `.secret/` directory is gitignored and contains machine-specific paths.

The historical design docs cover:
- Foundation design (game mechanics, core gameplay loop, setting)
- Deep technical detail (all data structures, AI integration, threading model)
- Sprint-by-sprint implementation roadmap (8+ sprints)

When starting a new session, read `README.md` first for the current v1 project direction and public contract. Then review the external design documents for deeper historical and architectural context. Use `docs/scenario-package-schema.md` and `docs/scenario-authoring-guide.md` when working on scenario package or creator-experience changes.

## Development Conventions

### Branching

Feature branches named `track/<alpha|beta|gamma|delta>/<feature-slug>`, merge to main requires CI green.

### Tracks

- **Track Alpha**: Foundation (entities, world, clock, events)
- **Track Beta**: AI layer (zoo-keeper-harness, prompt builder, tool registry, memory)
- **Track Gamma**: Engine/shell (command parser, game loop, renderer, save system)
- **Track Delta**: Content and scenario

### Code Style

- `.clang-format` enforces LLVM-based style with 4-space indent and 100-column limit
- C++23 features are expected and encouraged

### Testing

- Unit tests require no model and run in CI
- Integration tests are gated behind `CHRONICLE_INTEGRATION_LLM_BASE_URL` and `CHRONICLE_INTEGRATION_LLM_MODEL`

## Session Cleanup

At the end of each session, clean up any files generated under `docs/superpowers/` during that session. These are temporary planning/spec artifacts used during development and should not persist between sessions. The `docs/superpowers/` directory is gitignored.
