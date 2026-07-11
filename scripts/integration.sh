#!/usr/bin/env bash
# Optional live Ollama playthroughs (not part of default CI).
set -euo pipefail
# shellcheck disable=SC1091
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_lib.sh"
ensure_venv

echo "==> pytest (integration / Ollama)"
pytest -m integration -v "$@"
