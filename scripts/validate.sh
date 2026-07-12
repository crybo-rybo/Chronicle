#!/usr/bin/env bash
# Validate bundled example cartridges via the chronicle CLI.
set -euo pipefail
# shellcheck disable=SC1091
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_lib.sh"
ensure_venv

echo "==> chronicle validate examples"
chronicle validate --scenario examples/minimal
chronicle validate --scenario examples/broken_wheel
chronicle inspect --scenario examples/minimal >/dev/null
echo "OK"
