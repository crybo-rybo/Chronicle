# Contributing

## Toolchain

GCC 16 or newer (Chronicle uses C++26 reflection via `-freflection`), CMake
≥ 3.28, and libcurl development headers. All other dependencies are fetched by
CMake.

## Local CI (run before opening a PR)

```bash
just ci
```

That checks formatting, builds with warnings as errors, runs the unit tests,
and validates the bundled example cartridges. GitHub Actions additionally runs
the same suite with ASan/UBSan and smoke-tests a release installation.
Individual recipes:

| Recipe | Purpose |
| --- | --- |
| `just build` | Configure + build (Debug, tests on) |
| `just test` | Unit tests (no model) |
| `just validate` | Validate example cartridges |
| `just integration` | Live Ollama playthroughs (optional) |
| `just format` | clang-format over Chronicle sources |
| `just check-format` | Verify formatting without modifying files |

Without `just`: `cmake --preset ci && cmake --build --preset ci -j &&
ctest --preset ci`.

## Tests

Every mechanic change lands with unit tests (`tests/`); anything touching the
scry integration should also keep `tests/integration/` passing against a local
Ollama model (`just integration`, override the model with `CHRONICLE_MODEL`).

## Branching

Feature branches: `feature/<slug>` or `fix/<slug>`. Merge to `main` requires
CI green.
