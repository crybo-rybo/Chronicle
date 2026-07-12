#!/usr/bin/env bash
# Unit tests with coverage report (same gate CI uses).
set -euo pipefail
# shellcheck disable=SC1091
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_lib.sh"
ensure_venv

COVERAGE_MIN="${COVERAGE_MIN:-70}"

echo "==> pytest + coverage (min ${COVERAGE_MIN}%)"
pytest -m "not integration" \
  --cov=chronicle \
  --cov-report=term-missing \
  --cov-report=xml:coverage.xml \
  --cov-fail-under="${COVERAGE_MIN}" \
  -q \
  "$@"
