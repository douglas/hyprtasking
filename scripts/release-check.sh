#!/usr/bin/env bash
# Release gate. Runs each stage once and stops at the first failure.
#   RELEASE_MODE=full (default) | offline (no live compositor) | live
#   HYPRLAND_SOURCE=/path/to/Hyprland  also runs the compat audit
#   BUILD_DIR (default: build), SMOKE_MODE (default: stress)
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$REPO_DIR/build"}
RELEASE_MODE=${RELEASE_MODE:-full}
SMOKE_MODE=${SMOKE_MODE:-stress}
HYPRLAND_SOURCE=${HYPRLAND_SOURCE:-}

stage() {
  printf '\n== %s\n$ %s\n' "$1" "${*:2}"
  "${@:2}"
}

case "$RELEASE_MODE" in
  full | offline | live) ;;
  *)
    printf 'Invalid RELEASE_MODE: %s (expected full, offline, or live)\n' "$RELEASE_MODE" >&2
    exit 1
    ;;
esac

if [[ ! -d "$BUILD_DIR" ]]; then
  printf 'Build directory does not exist: %s\n' "$BUILD_DIR" >&2
  exit 1
fi

if [[ "$RELEASE_MODE" != "offline" ]]; then
  stage version-contract bash "$SCRIPT_DIR/check-version-contract.sh" ${HYPRLAND_SOURCE:+"$HYPRLAND_SOURCE"}
fi

if [[ "$RELEASE_MODE" != "live" ]]; then
  if [[ -n "$HYPRLAND_SOURCE" ]]; then
    stage compat bash "$SCRIPT_DIR/audit-compat.sh" "$HYPRLAND_SOURCE"
  fi
  stage source bash "$SCRIPT_DIR/audit-source.sh"
  stage compile meson compile -C "$BUILD_DIR"
  stage test meson test -C "$BUILD_DIR"
fi

if [[ "$RELEASE_MODE" != "offline" ]]; then
  stage smoke bash "$SCRIPT_DIR/smoke-live.sh" "$SMOKE_MODE"
  printf '\nFinish with the manual checks in docs/debugging-playbook.md#manual-checks.\n'
fi

printf '\nRelease check passed (%s mode).\n' "$RELEASE_MODE"
