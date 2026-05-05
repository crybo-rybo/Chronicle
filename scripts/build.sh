#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/build.sh [options] [-- <cmake build args>]

Configure and build Chronicle with a CMake preset.

Options:
  -p, --preset <name>   CMake preset to use. Defaults to CHRONICLE_PRESET or debug.
  -t, --target <name>   Build a specific CMake target.
      --clean-first     Ask CMake to clean before building.
  -h, --help            Show this help.

Examples:
  scripts/build.sh
  scripts/build.sh --preset debug-logging
  scripts/build.sh --target chronicle_tests
USAGE
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

preset="${CHRONICLE_PRESET:-debug}"
target=""
clean_first=0
extra_args=()

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
    -t | --target)
        if [[ $# -lt 2 ]]; then
            echo "error: --target requires a value" >&2
            exit 2
        fi
        target="$2"
        shift 2
        ;;
    --clean-first)
        clean_first=1
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    --)
        shift
        extra_args+=("$@")
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

cmake --preset "${preset}"

build_args=(--build --preset "${preset}" --parallel)
if [[ -n "${target}" ]]; then
    build_args+=(--target "${target}")
fi
if [[ "${clean_first}" -eq 1 ]]; then
    build_args+=(--clean-first)
fi
if [[ "${#extra_args[@]}" -gt 0 ]]; then
    build_args+=("${extra_args[@]}")
fi

cmake "${build_args[@]}"
