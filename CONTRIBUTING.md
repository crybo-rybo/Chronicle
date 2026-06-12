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

For dialogue and endpoint debugging, use the logging preset. It enables
Chronicle diagnostics:

```bash
cmake --preset debug-logging
cmake --build --preset debug-logging
CHRONICLE_LOG_FILE=chronicle.log ./build-logging/src/chronicle
```

`CHRONICLE_LOG_FILE` appends logs to the named file. `CHRONICLE_LOG_LEVEL`
accepts `debug`, `info`, `warning`, or `error`. `CHRONICLE_LOG=off` disables
Chronicle logging at runtime even in a logging build, while
`CHRONICLE_LOG=debug|info|warning|error` enables logging at that level.

Logs are local diagnostics, not release artifacts. Review or redact local paths,
player input, endpoint details, and scenario secrets before sharing a log
file outside your machine.

Manual harness smoke tests are opt-in because they require an
OpenAI-compatible endpoint:

```bash
cmake -B build -DCHRONICLE_BUILD_TOOLS=ON
cmake --build build --target harness_smoke_test --parallel
ZOO_BASE_URL=http://localhost:11434/v1 ZOO_MODEL=llama3.2 ./build/tools/harness_smoke_test
```

`ZOO_BASE_URL` defaults to `http://localhost:11434/v1`. `ZOO_MODEL` is
required. `ZOO_API_KEY` or `OPENAI_API_KEY` may be set when the endpoint
requires a key.

### Local LLM Endpoints

The bundled sample scenario keeps endpoint fields empty. Machine-local
endpoint URLs, model names, keys, and save paths belong in one of:

- A gitignored local config override, e.g. `.secret/local_config.json`
- Environment variables for one-off runs

Never commit endpoint credentials into tracked configuration. Shared cartridges
should also avoid committing endpoint URLs and model names unless the package
is intentionally bound to a specific local service.

Runtime config precedence is:

1. Scenario package `config.json`
2. Optional JSON file pointed at by `CHRONICLE_CONFIG_OVERRIDE`
3. Environment variables

The override JSON is partial, so it can contain only local fields:

```json
{
  "llm_base_url": "http://localhost:11434/v1",
  "llm_model": "llama3.2",
  "llm_api_key": "",
  "save_directory": ".secret/saves"
}
```

Use it like this:

```bash
CHRONICLE_CONFIG_OVERRIDE=.secret/local_config.json ./build/src/chronicle --scenario data
```

Supported runtime environment overrides are `ZOO_BASE_URL`, `ZOO_MODEL`,
`ZOO_API_KEY`, `OPENAI_API_KEY`, `CHRONICLE_LLM_BASE_URL`,
`CHRONICLE_LLM_MODEL`, `CHRONICLE_LLM_API_KEY`,
`CHRONICLE_LLM_ORGANIZATION`, `CHRONICLE_LLM_HTTP_TIMEOUT_MS`,
`CHRONICLE_LLM_MAX_RETRIES`, `CHRONICLE_LLM_TLS_VERIFY`,
`CHRONICLE_TEMPERATURE`, `CHRONICLE_MAX_RESPONSE_TOKENS`,
`CHRONICLE_INFERENCE_TIMEOUT_MS`, `CHRONICLE_SAVE_DIRECTORY`,
`CHRONICLE_USE_TUI`, `CHRONICLE_USE_COLOR`, and
`CHRONICLE_MAX_TOOL_ITERATIONS`.

Chronicle-specific `CHRONICLE_LLM_*` values win over the shorter `ZOO_*`
aliases. `OPENAI_API_KEY` is used only when neither `ZOO_API_KEY` nor
`CHRONICLE_LLM_API_KEY` is set.

### Integration Tests

Integration tests require an OpenAI-compatible endpoint and are gated behind a
CMake option:

```bash
cmake -B build -DCHRONICLE_INTEGRATION_TESTS=ON
cmake --build build --parallel
CHRONICLE_INTEGRATION_LLM_BASE_URL=http://localhost:11434/v1 \
CHRONICLE_INTEGRATION_LLM_MODEL=llama3.2 \
ctest --test-dir build --output-on-failure
```

With the default `CHRONICLE_INTEGRATION_TESTS=OFF`, integration tests are not
compiled. The integration tests also runtime-skip if endpoint variables are not
set, as a defense-in-depth guard. `ZOO_BASE_URL` and `ZOO_MODEL` are accepted
as shorter aliases for local runs.

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
