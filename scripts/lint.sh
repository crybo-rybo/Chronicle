#!/usr/bin/env bash
# Lint with Ruff (same check CI runs).
set -euo pipefail
# shellcheck disable=SC1091
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_lib.sh"
ensure_venv

echo "==> ruff check"
ruff check src tests

echo "==> ruff format --check"
ruff format --check src tests
