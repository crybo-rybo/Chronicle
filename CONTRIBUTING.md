# Contributing

## Local CI (run before opening a PR)

```bash
python3 -m venv .venv
source .venv/bin/activate
./scripts/ci.sh
```

`scripts/ci.sh` is what GitHub Actions runs. Individual checks:

| Script | Purpose |
| --- | --- |
| `./scripts/lint.sh` | Ruff lint + format check |
| `./scripts/format.sh` | Auto-fix lint/format |
| `./scripts/test.sh` | Unit tests (no model) |
| `./scripts/coverage.sh` | Unit tests + coverage gate |
| `./scripts/validate.sh` | Validate example cartridges |
| `./scripts/integration.sh` | Live Ollama playthroughs (optional) |

## Branching

Feature branches: `track/<alpha|beta|gamma|delta>/<feature-slug>`.
Merge to `main` requires CI green.
