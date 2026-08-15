#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$SCRIPT_DIR/common-hyprctl.sh"
if [[ -z "${PLUGIN_PATH:-}" ]]; then
  if configured_plugin_path="$(hyprtasking_configured_plugin_path)"; then
    PLUGIN_PATH="$configured_plugin_path"
  else
    PLUGIN_PATH="$SCRIPT_DIR/../build/libhyprtasking.so"
  fi
fi
PLUGIN_PATH=$(realpath "$PLUGIN_PATH")
MODE=${1:-all}
LOAD_UNLOAD_CYCLES=${LOAD_UNLOAD_CYCLES:-1}
TOGGLE_CYCLES=${TOGGLE_CYCLES:-1}
RELOAD_CYCLES=${RELOAD_CYCLES:-1}
PRINT_MANUAL_FOLLOW_UP=${PRINT_MANUAL_FOLLOW_UP:-1}
HYPRCTL_PREFIX=()
CAPTURED_OUTPUT=""

run() {
  printf '$ %s\n' "$*"
  "$@"
}

run_capture() {
  printf '$ %s\n' "$*"
  CAPTURED_OUTPUT="$("$@")"
  printf '%s\n' "$CAPTURED_OUTPUT"
}

init_hyprctl_prefix() {
  if ! hypr_set_prefix plugin list; then
    printf 'Unable to detect a live Hyprland instance.\n' >&2
    exit 1
  fi
}

run_hyprctl() {
  run "${HYPRCTL_PREFIX[@]}" "$@"
}

run_capture_hyprctl() {
  run_capture "${HYPRCTL_PREFIX[@]}" "$@"
}

hyprland_uses_lua_plugin_calls() {
  local version
  version="$("${HYPRCTL_PREFIX[@]}" version 2>/dev/null)"
  version="$(hypr_extract_semver "$version")" || return 1

  local major minor patch
  IFS=. read -r major minor patch <<< "$version"
  ((major > 0 || minor >= 56))
}

run_capture_hyprtasking() {
  local dispatcher=$1
  local arg=${2:-}
  local function=${dispatcher#hyprtasking:}
  local lua_code

  if hyprland_uses_lua_plugin_calls; then
    if [[ -n "$arg" ]]; then
      lua_code="hl.plugin.hyprtasking.${function}(\"${arg}\")"
    else
      lua_code="hl.plugin.hyprtasking.${function}()"
    fi

    printf '$ %s\n' "${HYPRCTL_PREFIX[*]} eval $lua_code"
    CAPTURED_OUTPUT="$("${HYPRCTL_PREFIX[@]}" eval "$lua_code" 2>&1)" || true
    printf '%s\n' "$CAPTURED_OUTPUT"
    return
  fi

  run_capture_hyprctl dispatch "$dispatcher" "$arg"
}

run_hyprtasking() {
  run_capture_hyprtasking "$@"
}

assert_plugin_loaded() {
  run_capture_hyprctl plugin list
  if ! printf '%s\n' "$CAPTURED_OUTPUT" | rg -q '^Plugin Hyprtasking by '; then
    printf 'Hyprtasking is not loaded.\n' >&2
    exit 1
  fi
}

fail_with_loaded_plugin_state() {
  local message=$1
  run_capture_hyprctl plugin list
  printf '%s\n' "$message" >&2
  printf 'Current plugin list:\n%s\n' "$CAPTURED_OUTPUT" >&2
  exit 1
}

assert_dispatcher_ready() {
  run_capture_hyprtasking hyprtasking:toggle __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    fail_with_loaded_plugin_state \
      "Hyprtasking is loaded, but its dispatchers are not registered. This usually means a stale or partially initialized instance is already resident."
  fi

  if [[ "$CAPTURED_OUTPUT" != *"invalid arg"* ]]; then
    printf 'Unexpected hyprtasking:toggle probe response: %s\n' "$CAPTURED_OUTPUT" >&2
    exit 1
  fi
}

assert_extended_dispatchers_ready() {
  assert_dispatcher_ready

  run_capture_hyprtasking hyprtasking:move __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    fail_with_loaded_plugin_state \
      "Hyprtasking move dispatcher is not registered even though the plugin is listed."
  fi

  if [[ "$CAPTURED_OUTPUT" != "ok" ]]; then
    printf 'Unexpected hyprtasking:move probe response: %s\n' "$CAPTURED_OUTPUT" >&2
    exit 1
  fi

  run_capture_hyprtasking hyprtasking:select __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    fail_with_loaded_plugin_state \
      "Hyprtasking select dispatcher is not registered even though the plugin is listed."
  fi

  run_capture_hyprtasking hyprtasking:commit
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    fail_with_loaded_plugin_state \
      "Hyprtasking commit dispatcher is not registered even though the plugin is listed."
  fi

  run_capture_hyprtasking hyprtasking:health json
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    fail_with_loaded_plugin_state \
      "Hyprtasking health dispatcher is not registered even though the plugin is listed."
  fi

  if [[ "$CAPTURED_OUTPUT" != "ok" ]]; then
    printf 'Unexpected hyprtasking:health probe response: %s\n' "$CAPTURED_OUTPUT" >&2
    exit 1
  fi
}

smoke_dispatchers() {
  assert_plugin_loaded
  assert_extended_dispatchers_ready
}

assert_runtime_disabled() {
  run_capture_hyprtasking hyprtasking:toggle __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" != *"runtime disabled"* ]]; then
    printf 'Expected runtime to be disabled, got: %s\n' "$CAPTURED_OUTPUT" >&2
    exit 1
  fi
}

set_safety_option() {
  local key=$1
  local value=$2
  run_hyprctl keyword "plugin:hyprtasking:${key}" "$value"
}

restore_safe_options() {
  run_hyprctl keyword plugin:hyprtasking:drag_button 272
  run_hyprctl keyword plugin:hyprtasking:select_button 273
  run_hyprctl keyword plugin:hyprtasking:gestures:move_fingers 3
  run_hyprctl keyword plugin:hyprtasking:gestures:open_fingers 4
  run_hyprctl keyword plugin:hyprtasking:grid:rows 3
  run_hyprctl keyword plugin:hyprtasking:grid:cols 3
  run_hyprctl keyword plugin:hyprtasking:gestures:move_distance 300
  run_hyprctl keyword plugin:hyprtasking:gestures:open_distance 300
  run_capture_hyprctl plugin unload "$PLUGIN_PATH"
  run_capture_hyprctl plugin load "$PLUGIN_PATH"
  wait_for_plugin_ready
  smoke_dispatchers
}

smoke_runtime_safety() {
  assert_plugin_loaded
  restore_safe_options

  set_safety_option "grid:rows" 0
  assert_runtime_disabled

  restore_safe_options

  set_safety_option "grid:cols" 0
  assert_runtime_disabled

  restore_safe_options

  set_safety_option "gestures:move_distance" 0
  assert_runtime_disabled

  restore_safe_options

  set_safety_option "gestures:open_distance" 0
  assert_runtime_disabled

  restore_safe_options

  set_safety_option "drag_button" 272
  set_safety_option "select_button" 272
  assert_runtime_disabled

  restore_safe_options

  set_safety_option "gestures:move_fingers" 3
  set_safety_option "gestures:open_fingers" 3
  assert_runtime_disabled

  restore_safe_options
}

wait_for_plugin_ready() {
  local attempts=${1:-40}
  local delay=${2:-0.1}
  local i
  local saw_plugin=0

  for ((i = 0; i < attempts; i++)); do
    run_capture_hyprctl plugin list
    if printf '%s\n' "$CAPTURED_OUTPUT" | rg -q '^Plugin Hyprtasking by '; then
      saw_plugin=1
      run_capture_hyprtasking hyprtasking:toggle __smoke_probe__
      if [[ "$CAPTURED_OUTPUT" != *"Invalid dispatcher"* ]]; then
        return 0
      fi
    fi
    sleep "$delay"
  done

  if ((saw_plugin != 0)); then
    fail_with_loaded_plugin_state \
      "Hyprtasking remained listed, but its dispatchers never registered. This usually means the new plugin path failed to initialize and an older instance is still loaded."
  fi

  printf 'Timed out waiting for Hyprtasking to finish loading.\n' >&2
  exit 1
}

if ! command -v hyprctl >/dev/null 2>&1; then
  printf 'hyprctl is required for the live smoke test.\n' >&2
  exit 1
fi

init_hyprctl_prefix

smoke_toggle() {
  local cycle
  for ((cycle = 0; cycle < TOGGLE_CYCLES; cycle++)); do
    smoke_dispatchers
    run_hyprtasking hyprtasking:toggle cursor
    run_hyprtasking hyprtasking:move right
    run_hyprtasking hyprtasking:move left
    run_hyprtasking hyprtasking:toggle cursor
  done
  run_hyprctl plugin list
}

smoke_reload() {
  local cycle
  for ((cycle = 0; cycle < RELOAD_CYCLES; cycle++)); do
    assert_plugin_loaded
    run_hyprctl reload
    wait_for_plugin_ready
    smoke_dispatchers
  done
}

smoke_reload_open() {
  local cycle
  for ((cycle = 0; cycle < RELOAD_CYCLES; cycle++)); do
    smoke_dispatchers
    run_hyprtasking hyprtasking:toggle cursor
    run_hyprctl reload
    wait_for_plugin_ready
    smoke_dispatchers
    run_hyprtasking hyprtasking:toggle cursor
    run_hyprtasking hyprtasking:toggle cursor
  done
}

smoke_stress() {
  local saved_load_cycles=${LOAD_UNLOAD_CYCLES}
  local saved_toggle_cycles=${TOGGLE_CYCLES}
  local saved_reload_cycles=${RELOAD_CYCLES}

  if [[ "$LOAD_UNLOAD_CYCLES" == "1" ]]; then
    LOAD_UNLOAD_CYCLES=3
  fi
  if [[ "$TOGGLE_CYCLES" == "1" ]]; then
    TOGGLE_CYCLES=3
  fi
  if [[ "$RELOAD_CYCLES" == "1" ]]; then
    RELOAD_CYCLES=2
  fi

  smoke_load_unload
  smoke_toggle
  smoke_reload
  smoke_reload_open

  LOAD_UNLOAD_CYCLES=${saved_load_cycles}
  TOGGLE_CYCLES=${saved_toggle_cycles}
  RELOAD_CYCLES=${saved_reload_cycles}
}

smoke_load_unload() {
  if [[ ! -f "$PLUGIN_PATH" ]]; then
    printf 'Plugin path does not exist: %s\n' "$PLUGIN_PATH" >&2
    exit 1
  fi

  local cycle
  for ((cycle = 0; cycle < LOAD_UNLOAD_CYCLES; cycle++)); do
    run_capture_hyprctl plugin unload "$PLUGIN_PATH"
    if [[ "$CAPTURED_OUTPUT" == *"plugin not loaded"* ]]; then
      run_capture_hyprctl plugin list
      if printf '%s\n' "$CAPTURED_OUTPUT" | rg -q '^Plugin Hyprtasking by '; then
        printf 'Target plugin path is not the currently loaded Hyprtasking instance: %s\n' "$PLUGIN_PATH" >&2
        printf 'A different Hyprtasking instance is already loaded.\n' >&2
        printf 'Current plugin list:\n%s\n' "$CAPTURED_OUTPUT" >&2
        exit 1
      fi
    fi

    run_capture_hyprctl plugin load "$PLUGIN_PATH"
    if [[ "$CAPTURED_OUTPUT" == *"could not be loaded:"* ]]; then
      printf 'Failed to load target plugin path: %s\n' "$PLUGIN_PATH" >&2
      printf '%s\n' "$CAPTURED_OUTPUT" >&2
      exit 1
    fi

    wait_for_plugin_ready
    assert_plugin_loaded
  done
}

run_manual_follow_up() {
  run bash "$SCRIPT_DIR/manual-runtime-check.sh" all
}

print_manual_follow_up_if_enabled() {
  if [[ "$PRINT_MANUAL_FOLLOW_UP" == "1" ]]; then
    run_manual_follow_up
  fi
}

case "$MODE" in
  all)
    smoke_load_unload
    smoke_dispatchers
    smoke_runtime_safety
    smoke_toggle
    smoke_reload
    smoke_reload_open
    print_manual_follow_up_if_enabled
    ;;
  stress)
    smoke_stress
    print_manual_follow_up_if_enabled
    ;;
  dispatchers)
    smoke_dispatchers
    ;;
  safety)
    smoke_runtime_safety
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
  reload-open)
    smoke_reload_open
    ;;
  manual)
    assert_plugin_loaded
    run bash "$SCRIPT_DIR/manual-runtime-check.sh" "${2:-all}"
    ;;
  *)
    printf 'Usage: %s [all|stress|dispatchers|safety|load-unload|toggle|move|reload|reload-open|manual [all|drag|typing-focus|gesture|reload-open|monitor-remove|render-reentry|hook-sunset]]\n' "$0" >&2
    exit 1
    ;;
esac
