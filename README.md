# Chronicle

Chronicle is a bounded scenario console/runtime for offline, LLM-driven NPC
mystery and social-sim text adventures. Chronicle is the console: it owns the
C++23 runtime, local
[zoo-keeper-harness](https://github.com/crybo-rybo/zoo-keeper-harness)
integration for OpenAI-compatible endpoints, prompt assembly, save/load,
validation, and strict tool/mutation pipeline. Creators bring the cartridges:
JSON scenario packages that define a world, cast, facts, flags, events, and
runtime defaults.

The stable v1 public contract is deliberately narrow:

- CLI runner: `chronicle [--scenario <dir>]`, `chronicle run <id>`, `chronicle list`
- CLI library: `chronicle install <path>`, `chronicle pack --scenario <dir>`
- CLI inspector: `chronicle inspect --scenario <dir>`
- CLI validator: `chronicle validate --scenario <dir>`
- JSON scenario package schema and validation behavior
- Console mechanics catalog: [`docs/console-api.md`](docs/console-api.md)

The bundled `data/` directory is a sample cartridge and contract fixture, not
the product itself. C++ headers, source layout, and library boundaries are
implementation details for v1 and may change without compatibility guarantees.

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

For a local debug build with Chronicle diagnostics enabled:

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

Inspect cartridge identity and readiness:

```bash
./build/src/chronicle inspect --scenario data
./build/src/chronicle inspect --scenario examples/minimal_scenario
```

Validate a scenario package without starting play:

```bash
./build/src/chronicle validate --scenario data
./build/src/chronicle validate --scenario examples/minimal_scenario
./build/src/chronicle validate --scenario examples/lighthouse_veil
```

Manage the cartridge library:

```bash
./build/src/chronicle list
./build/src/chronicle install examples/minimal_scenario
./build/src/chronicle run minimal_scenario
./build/src/chronicle pack --scenario examples/minimal_scenario --output /tmp/minimal.chronicle
```

Once the runtime starts, type `help` for the in-game command list.

Tracked scenario packages keep LLM endpoint settings empty, so NPC dialogue
uses explicit stub output until you configure an OpenAI-compatible endpoint for
your machine. This is expected for a fresh checkout; deterministic commands,
validation, save/load, and scripted events still work. See
[Local LLM Endpoints](CONTRIBUTING.md#local-llm-endpoints).

For a one-off model-backed run against a local endpoint:

```bash
ZOO_BASE_URL=http://localhost:11434/v1 ZOO_MODEL=llama3.2 ./build/src/chronicle --scenario data
```

For repeatable local settings, put partial overrides in a gitignored JSON file
and point `CHRONICLE_CONFIG_OVERRIDE` at it:

```bash
CHRONICLE_CONFIG_OVERRIDE=.secret/local_config.json ./build/src/chronicle --scenario data
```

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

A scenario package is Chronicle's cartridge format: a directory containing
`scenario.json` plus JSON files for config, world, NPCs, facts, flags, and
events. The bundled `data/` directory is the canonical small sample cartridge.
`examples/minimal_scenario/` is the smallest copyable starter, while
`examples/lighthouse_veil/` demonstrates richer locked-exit, multi-NPC, and
event-chain mechanics.

`scenario.json` is the cartridge manifest. It declares package identity,
content version, Chronicle schema compatibility, descriptive metadata, and file
paths:

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
    "description": "A small Chronicle scenario cartridge.",
    "author": "Chronicle Contributors",
    "license": "MIT"
  }
}
```

Manifest file paths must be relative paths that stay inside the package directory. Chronicle v1 treats all six files as required package inputs.

Before sharing a cartridge, run `chronicle inspect --scenario <dir>` to review
identity and readiness, then `chronicle validate --scenario <dir>` for the
endpoint-free validation gate. Shared cartridges should not commit endpoint
URLs, model names, or credentials; operators provide those with environment
variables or a gitignored override file.

NPCs may declare per-character tool policies in `npcs.json`. `allowed_tools` is the fixed v1 tool palette; scoped lists restrict which authored IDs the NPC may touch. Empty scoped lists mean no additional ID restriction.

Supported NPC tools: `say`, `give_item`, `take_item`, `update_mood`, `update_trust`, `move_self`, `reveal_knowledge`, `remember`, `set_flag`, `inspect_item`.

For the field-by-field package reference, see [`docs/scenario-package-schema.md`](docs/scenario-package-schema.md). For authoring guidance, see [`docs/scenario-authoring-guide.md`](docs/scenario-authoring-guide.md). For the runtime mechanics catalog, see [`docs/console-api.md`](docs/console-api.md). JSON Schema files for editor integration live under [`schemas/`](schemas).

## Testing

```bash
ctest --test-dir build --output-on-failure
```

Integration tests are compiled only when requested and runtime-skip unless an
endpoint is configured:

```bash
cmake -B build-integration -DCHRONICLE_INTEGRATION_TESTS=ON
cmake --build build-integration --parallel
CHRONICLE_INTEGRATION_LLM_BASE_URL=http://localhost:11434/v1 \
CHRONICLE_INTEGRATION_LLM_MODEL=llama3.2 \
ctest --test-dir build-integration --output-on-failure
```

## Project Structure

```
src/           C++ source (engine, world, scenario, AI, rendering, persistence)
data/          Bundled sample scenario package (JSON)
examples/      Copyable and richer reference scenario packages
tests/         Unit and integration tests
docs/          Design documents and specifications
```
