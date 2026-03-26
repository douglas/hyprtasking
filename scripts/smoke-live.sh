#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PLUGIN_PATH=${PLUGIN_PATH:-"$SCRIPT_DIR/../build/libhyprtasking.so"}
PLUGIN_PATH=$(realpath "$PLUGIN_PATH")
MODE=${1:-all}

run() {
  printf '$ %s\n' "$*"
  "$@"
}

run_capture() {
  printf '$ %s\n' "$*"
  CAPTURED_OUTPUT="$("$@")"
  printf '%s\n' "$CAPTURED_OUTPUT"
}

assert_plugin_loaded() {
  run_capture hyprctl plugin list
  if ! printf '%s\n' "$CAPTURED_OUTPUT" | rg -q '^Plugin Hyprtasking by '; then
    printf 'Hyprtasking is not loaded.\n' >&2
    exit 1
  fi
}

assert_dispatcher_ready() {
  run_capture hyprctl dispatch hyprtasking:toggle __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    printf 'Hyprtasking dispatchers are not registered yet.\n' >&2
    exit 1
  fi
}

wait_for_plugin_ready() {
  local attempts=${1:-40}
  local delay=${2:-0.1}
  local i

  for ((i = 0; i < attempts; i++)); do
    run_capture hyprctl plugin list
    if printf '%s\n' "$CAPTURED_OUTPUT" | rg -q '^Plugin Hyprtasking by '; then
      run_capture hyprctl dispatch hyprtasking:toggle __smoke_probe__
      if [[ "$CAPTURED_OUTPUT" != *"Invalid dispatcher"* ]]; then
        return 0
      fi
    fi
    sleep "$delay"
  done

  printf 'Timed out waiting for Hyprtasking to finish loading.\n' >&2
  exit 1
}

if ! command -v hyprctl >/dev/null 2>&1; then
  printf 'hyprctl is required for the live smoke test.\n' >&2
  exit 1
fi

smoke_toggle() {
  assert_plugin_loaded
  assert_dispatcher_ready
  run hyprctl dispatch hyprtasking:toggle cursor
  run hyprctl dispatch hyprtasking:move right
  run hyprctl dispatch hyprtasking:move left
  run hyprctl dispatch hyprtasking:toggle cursor
  run hyprctl plugin list
}

smoke_reload() {
  assert_plugin_loaded
  run hyprctl reload
  wait_for_plugin_ready
  assert_plugin_loaded
  assert_dispatcher_ready
}

smoke_load_unload() {
  if [[ ! -f "$PLUGIN_PATH" ]]; then
    printf 'Plugin path does not exist: %s\n' "$PLUGIN_PATH" >&2
    exit 1
  fi

  run hyprctl plugin unload "$PLUGIN_PATH"
  run hyprctl plugin load "$PLUGIN_PATH"
  wait_for_plugin_ready
  assert_plugin_loaded
}

print_manual_follow_up() {
  printf '\nManual follow-up:\n'
  printf '1. Drag a tiled window across workspaces in the overview.\n'
  printf '2. Move a hovered window with hyprtasking:movewindow.\n'
  printf '3. Trigger a gesture open/close sequence.\n'
  printf '4. Reload Hyprland while the overview is open.\n'
}

case "$MODE" in
  all)
    smoke_load_unload
    smoke_toggle
    smoke_reload
    print_manual_follow_up
    ;;
  load-unload)
    smoke_load_unload
    ;;
  toggle|move)
    smoke_toggle
    ;;
  reload)
    smoke_reload
    ;;
  manual)
    assert_plugin_loaded
    print_manual_follow_up
    ;;
  *)
    printf 'Usage: %s [all|load-unload|toggle|move|reload|manual]\n' "$0" >&2
    exit 1
    ;;
esac
