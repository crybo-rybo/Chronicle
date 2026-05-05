#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/test.sh [options] [-- <ctest args>]

Build and run Chronicle's default non-model test suite.

Options:
  -p, --preset <name>   CMake preset to use. Defaults to CHRONICLE_PRESET or debug.
      --no-build        Skip the build step and run ctest only.
  -R, --filter <regex>  Run tests matching a CTest regex.
  -V, --verbose         Show verbose CTest output.
  -h, --help            Show this help.

Examples:
  scripts/test.sh
  scripts/test.sh --filter CommandParser
  scripts/test.sh -- --rerun-failed
USAGE
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

preset="${CHRONICLE_PRESET:-debug}"
skip_build=0
ctest_args=(--output-on-failure)

while [[ $# -gt 0 ]]; do
    case "$1" in
    -p | --preset)
        if [[ $# -lt 2 ]]; then
            echo "error: --preset requires a value" >&2
            exit 2
        fi
        preset="$2"
        shift 2
        ;;
    --no-build)
        skip_build=1
        shift
        ;;
    -R | --filter)
        if [[ $# -lt 2 ]]; then
            echo "error: --filter requires a value" >&2
            exit 2
        fi
        ctest_args+=("-R" "$2")
        shift 2
        ;;
    -V | --verbose)
        ctest_args+=("--verbose")
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    --)
        shift
        ctest_args+=("$@")
        break
        ;;
    *)
        echo "error: unknown argument: $1" >&2
        usage >&2
        exit 2
        ;;
    esac
done

cd "${repo_root}"

if [[ "${skip_build}" -eq 0 ]]; then
    "${repo_root}/scripts/build.sh" --preset "${preset}"
fi

ctest --preset "${preset}" "${ctest_args[@]}"
