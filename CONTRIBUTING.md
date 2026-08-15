# Contributing

## Toolchain

GCC 16 or newer (Chronicle uses C++26 reflection via `-freflection`), CMake
≥ 3.25, and libcurl development headers. All other dependencies are fetched by
CMake.

## Local CI (run before opening a PR)

```bash
just ci
```

That builds the `dev` preset, runs the unit tests, and validates the bundled
example cartridges — the same checks GitHub Actions runs. Individual recipes:

| Recipe | Purpose |
| --- | --- |
| `just build` | Configure + build (Debug, tests on) |
| `just test` | Unit tests (no model) |
| `just validate` | Validate example cartridges |
| `just integration` | Live Ollama playthroughs (optional) |
| `just format` | clang-format over Chronicle sources |

Without `just`: `cmake --preset dev && cmake --build --preset dev -j &&
ctest --preset dev`.

## Tests

Every mechanic change lands with unit tests (`tests/`); anything touching the
scry integration should also keep `tests/integration/` passing against a local
Ollama model (`just integration`, override the model with `CHRONICLE_MODEL`).

## Branching

Feature branches: `feature/<slug>` or `fix/<slug>`. Merge to `main` requires
CI green.
