#!/usr/bin/env bash
# Full local pre-PR check — mirrors GitHub Actions CI.
set -euo pipefail
# shellcheck disable=SC1091
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_lib.sh"

echo "==> install editable package + dev deps"
ensure_dev_install

./scripts/lint.sh
./scripts/coverage.sh
./scripts/validate.sh

echo
echo "CI checks passed. Optional live LLM: ./scripts/integration.sh"
