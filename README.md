# Chronicle

Chronicle is a bounded scenario SDK/runtime for offline, LLM-driven NPC mystery and social-sim text adventures. Creators author JSON scenario packages; Chronicle provides the C++23 runtime, local [Zoo-Keeper](https://github.com/crybo-rybo/zoo-keeper) integration, prompt assembly, save/load, validation, and a strict tool/mutation pipeline.

The stable v1 public contract is deliberately narrow:

- CLI runner: `chronicle [--scenario <dir>]`
- CLI validator: `chronicle validate --scenario <dir>`
- JSON scenario package schema and validation behavior

The bundled `data/` directory is a sample package and contract fixture, not the product itself. C++ headers, source layout, and library boundaries are implementation details for v1 and may change without compatibility guarantees.

## Prerequisites

- CMake 3.25+
- C++23-capable compiler (GCC 13+, Clang 17+, or Apple Clang via Xcode 15+)
## Building

```bash
git clone https://github.com/crybo-rybo/chronicle.git
cd chronicle
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

For a local debug build with Chronicle and Zoo-Keeper diagnostics enabled:

```bash
cmake --preset debug-logging
cmake --build --preset debug-logging
```

For day-to-day local development, the repo also includes convenience wrappers:

```bash
scripts/build.sh
scripts/format.sh
scripts/test.sh
```

## Running

Print the stable CLI surface:

```bash
./build/src/chronicle --help
```

Run the bundled sample scenario:

```bash
./build/src/chronicle
```

Run a scenario package explicitly:

```bash
./build/src/chronicle --scenario data
```

Validate a scenario package without starting play:

```bash
./build/src/chronicle validate --scenario data
./build/src/chronicle validate --scenario examples/minimal_scenario
./build/src/chronicle validate --scenario examples/lighthouse_veil
```

Once the runtime starts, type `help` for the in-game command list.

Tracked scenario packages keep `model_path` empty, so NPC dialogue uses explicit
stub output until you configure a local GGUF model path for your machine. This
is expected for a fresh checkout; deterministic commands, validation, save/load,
and scripted events still work. See [Local Model Paths](CONTRIBUTING.md#local-model-paths).

## Logging

Logging builds default to stderr. To keep the terminal UI clean, send logs to a file:

```bash
CHRONICLE_LOG_FILE=chronicle.log ./build-logging/src/chronicle
```

Runtime logging controls:

- `CHRONICLE_LOG_FILE=/path/to/chronicle.log` appends Chronicle logs to a file.
- `CHRONICLE_LOG_LEVEL=debug|info|warning|error` sets the minimum emitted level.
- `CHRONICLE_LOG=off` disables Chronicle logging even in a logging build.
- `CHRONICLE_LOG=debug|info|warning|error` enables logging at that level.

Logs are local diagnostics. Before sharing them, review or redact local file
paths, player input, model setup details, and any scenario secrets that may
appear in debug output.

## Scenario Packages

A scenario package is a directory containing `scenario.json` plus JSON files for
config, world, NPCs, facts, flags, and events. The bundled `data/` directory is
the canonical small sample package. `examples/minimal_scenario/` is the
smallest copyable starter, while `examples/lighthouse_veil/` demonstrates
richer locked-exit, multi-NPC, and event-chain mechanics.

`scenario.json` declares package metadata and file paths:

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
  }
}
```

Manifest file paths must be relative paths that stay inside the package directory. Chronicle v1 treats all six files as required package inputs.

NPCs may declare per-character tool policies in `npcs.json`. `allowed_tools` is the fixed v1 tool palette; scoped lists restrict which authored IDs the NPC may touch. Empty scoped lists mean no additional ID restriction.

Supported NPC tools: `say`, `give_item`, `take_item`, `update_mood`, `update_trust`, `move_self`, `reveal_knowledge`, `remember`, `set_flag`, `inspect_item`.

For the field-by-field package reference, see [`docs/scenario-package-schema.md`](docs/scenario-package-schema.md). For authoring guidance, see [`docs/scenario-authoring-guide.md`](docs/scenario-authoring-guide.md). JSON Schema files for editor integration live under [`schemas/`](schemas).

## Testing

```bash
ctest --test-dir build --output-on-failure
```

## Project Structure

```
src/           C++ source code (engine, entities, AI, rendering, persistence)
data/          Bundled sample scenario package (JSON)
examples/      Copyable and richer reference scenario packages
tests/         Unit and integration tests
docs/          Design documents and specifications
```
