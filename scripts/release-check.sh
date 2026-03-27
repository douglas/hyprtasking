#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$REPO_DIR/build"}
SMOKE_MODE=${SMOKE_MODE:-stress}
PRINT_MANUAL_CHECKLIST=${PRINT_MANUAL_CHECKLIST:-1}

run() {
  printf '$ %s\n' "$*"
  "$@"
}

if [[ ! -d "$BUILD_DIR" ]]; then
  printf 'Build directory does not exist: %s\n' "$BUILD_DIR" >&2
  exit 1
fi

run meson compile -C "$BUILD_DIR"
run meson test -C "$BUILD_DIR"
run env "PRINT_MANUAL_FOLLOW_UP=0" bash "$SCRIPT_DIR/smoke-live.sh" "$SMOKE_MODE"

if [[ "$PRINT_MANUAL_CHECKLIST" == "1" ]]; then
  run bash "$SCRIPT_DIR/manual-runtime-check.sh"
fi
