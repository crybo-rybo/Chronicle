#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/format.sh [--check] [path ...]

Format Chronicle C++ sources with clang-format.

Options:
      --check   Verify formatting without modifying files.
  -h, --help    Show this help.

When no paths are provided, tracked C++ files under src/, tests/, and tools/
are formatted.
USAGE
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

mode="write"
paths=()

while [[ $# -gt 0 ]]; do
    case "$1" in
    --check)
        mode="check"
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    --)
        shift
        paths+=("$@")
        break
        ;;
    -*)
        echo "error: unknown argument: $1" >&2
        usage >&2
        exit 2
        ;;
    *)
        paths+=("$1")
        shift
        ;;
    esac
done

if ! command -v clang-format >/dev/null 2>&1; then
    echo "error: clang-format is not installed or not on PATH" >&2
    exit 127
fi

cd "${repo_root}"

if [[ "${#paths[@]}" -gt 0 ]]; then
    if [[ "${mode}" == "check" ]]; then
        clang-format --dry-run --Werror "${paths[@]}"
    else
        clang-format -i "${paths[@]}"
    fi
    exit 0
fi

pathspecs=(
    "src/*.cpp"
    "src/*.hpp"
    "tests/*.cpp"
    "tests/*.hpp"
    "tools/*.cpp"
    "tools/*.hpp"
)

if [[ "${mode}" == "check" ]]; then
    git ls-files -z -- "${pathspecs[@]}" | xargs -0 clang-format --dry-run --Werror
else
    git ls-files -z -- "${pathspecs[@]}" | xargs -0 clang-format -i
fi
