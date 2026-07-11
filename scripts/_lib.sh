#!/usr/bin/env bash
# Shared helpers for Chronicle CI scripts.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ensure_venv() {
  if [[ -n "${VIRTUAL_ENV:-}" ]]; then
    return 0
  fi
  if [[ -x "$ROOT/.venv/bin/python" ]]; then
    # shellcheck disable=SC1091
    source "$ROOT/.venv/bin/activate"
    return 0
  fi
  return 0
}

ensure_dev_install() {
  ensure_venv
  python -m pip install -e ".[dev]" -q
}
