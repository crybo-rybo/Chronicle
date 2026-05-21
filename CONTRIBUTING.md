# Contributing

Chronicle is a C++23 bounded scenario SDK/runtime for offline, LLM-driven NPC mystery
and social-sim text adventures. Authors create JSON scenario packages; Chronicle runs
and validates them with deterministic engine state and LLM-driven NPC behavior. The v1
public contract is the CLI plus the JSON scenario package schema; C++ APIs remain
implementation details. Keep changes small, covered by tests, and aligned with the
roadmap.

## Build And Test

Use CMake 3.25 or newer with a C++23 compiler.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For dialogue/model debugging, use the logging preset. It enables Chronicle
diagnostics and turns on Zoo-Keeper logging by default:

```bash
cmake --preset debug-logging
cmake --build --preset debug-logging
CHRONICLE_LOG_FILE=chronicle.log ./build-logging/src/chronicle
```

`CHRONICLE_LOG_FILE` appends logs to the named file. `CHRONICLE_LOG_LEVEL`
accepts `debug`, `info`, `warning`, or `error`. `CHRONICLE_LOG=off` disables
Chronicle logging at runtime even in a logging build, while
`CHRONICLE_LOG=debug|info|warning|error` enables logging at that level. Pass
`-DCHRONICLE_ENABLE_ZOO_LOGGING=OFF` during configure if you need Chronicle
diagnostics without Zoo-Keeper diagnostics.

Logs are local diagnostics, not release artifacts. Review or redact local paths,
player input, model setup details, and scenario secrets before sharing a log
file outside your machine.

Manual Zoo-Keeper smoke tests are opt-in because they require a local model:

```bash
cmake -B build -DCHRONICLE_BUILD_TOOLS=ON
cmake --build build --target zk_smoke_test --parallel
ZOO_MODEL_PATH=/path/to/model.gguf ./build/tools/zk_smoke_test
```

`ZOO_MODEL_PATH` must point to a local GGUF model used by `tools/zk_smoke_test`.
Automated integration tests that need a model use the separate
`ZOO_INTEGRATION_MODEL` environment variable.

### Local Model Paths

The bundled sample scenario keeps `data/config.json` with `model_path` empty.
Machine-local model paths and hardware-specific runtime settings belong in one of:

- A gitignored local config override, e.g. `.secret/local_config.json`
- Environment variables: `ZOO_MODEL_PATH` for normal runs or
  `ZOO_INTEGRATION_MODEL` for integration tests

Never commit a machine-local path into tracked configuration.

Runtime config precedence is:

1. Scenario package `config.json`
2. Optional JSON file pointed at by `CHRONICLE_CONFIG_OVERRIDE`
3. Environment variables

The override JSON is partial, so it can contain only local fields:

```json
{
  "model_path": "/path/to/model.gguf",
  "n_gpu_layers": -1,
  "save_directory": ".secret/saves"
}
```

Use it like this:

```bash
CHRONICLE_CONFIG_OVERRIDE=.secret/local_config.json ./build/src/chronicle --scenario data
```

Supported runtime environment overrides are `ZOO_MODEL_PATH`,
`CHRONICLE_MODEL_PATH`, `CHRONICLE_CONTEXT_SIZE`, `CHRONICLE_N_GPU_LAYERS`,
`CHRONICLE_TEMPERATURE`, `CHRONICLE_MAX_RESPONSE_TOKENS`,
`CHRONICLE_INFERENCE_TIMEOUT_MS`, `CHRONICLE_SAVE_DIRECTORY`,
`CHRONICLE_USE_TUI`, `CHRONICLE_USE_COLOR`, and
`CHRONICLE_MAX_TOOL_ITERATIONS`. If both `ZOO_MODEL_PATH` and
`CHRONICLE_MODEL_PATH` are set, `CHRONICLE_MODEL_PATH` wins.

### Integration Tests

Integration tests require a local LLM model and are gated behind a CMake option:

```bash
cmake -B build -DCHRONICLE_INTEGRATION_TESTS=ON
cmake --build build --parallel
ZOO_INTEGRATION_MODEL=/path/to/model.gguf ctest --test-dir build --output-on-failure
```

With the default `CHRONICLE_INTEGRATION_TESTS=OFF`, integration tests are not
compiled. The integration tests also runtime-skip if `ZOO_INTEGRATION_MODEL`
(or `ZOO_MODEL_PATH`) is not set, as a defense-in-depth guard.

## Style

- Types use `PascalCase`.
- Functions, variables, data members, and file names use `snake_case`.
- Private data members end with `_`.
- Headers use `#pragma once`.
- Includes are ordered with the matching project header first, then project headers,
  then standard or third-party headers.
- Formatting is controlled by `.clang-format`; run it before committing C++ changes.

## Architecture Constraints

`GameEngine` is the single mutation gate for `World`. Other systems, including AI
tool handling, must not mutate `World` directly. They submit `MutationRequest`
values for validation and deterministic application by the engine.

The AI layer may read world state to build prompts, but it must not own or write
authoritative game state.
