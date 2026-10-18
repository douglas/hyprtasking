#!/usr/bin/env bash
# Live smoke checks against a running Hyprland. Modes:
#   all          load/unload, dispatcher probes, config safety, toggle/move, reloads
#   stress       all, with more cycles (defaults 3/3/2 instead of 1/1/1)
#   dispatchers  check toggle/move/select/commit/health are registered
#   safety       invalid config values are rejected while the runtime stays enabled
#   load-unload | toggle | reload | reload-open   run one stage
# Environment: HYPRLAND_INSTANCE_SIGNATURE, PLUGIN_PATH, LOAD_UNLOAD_CYCLES,
# TOGGLE_CYCLES, RELOAD_CYCLES.
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
if [[ "$MODE" == "stress" ]]; then
  LOAD_UNLOAD_CYCLES=${LOAD_UNLOAD_CYCLES:-3}
  TOGGLE_CYCLES=${TOGGLE_CYCLES:-3}
  RELOAD_CYCLES=${RELOAD_CYCLES:-2}
else
  LOAD_UNLOAD_CYCLES=${LOAD_UNLOAD_CYCLES:-1}
  TOGGLE_CYCLES=${TOGGLE_CYCLES:-1}
  RELOAD_CYCLES=${RELOAD_CYCLES:-1}
fi
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

  local major minor
  IFS=. read -r major minor _ <<< "$version"
  ((major > 0 || minor >= 56))
}

run_hyprtasking() {
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

# True when `hyprctl plugin list` (left in CAPTURED_OUTPUT) shows Hyprtasking.
plugin_listed() {
  run_capture_hyprctl plugin list
  printf '%s\n' "$CAPTURED_OUTPUT" | rg -q '^Plugin Hyprtasking by '
}

assert_plugin_loaded() {
  if ! plugin_listed; then
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

# probe NAME: calls hyprtasking:NAME with a dummy argument and fails if the
# dispatcher is not registered.
probe() {
  run_hyprtasking "hyprtasking:$1" __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    fail_with_loaded_plugin_state \
      "Hyprtasking $1 dispatcher is not registered even though the plugin is listed. This usually means a stale or partially initialized instance is already resident."
  fi
}

# probe_rejects_arg NAME: like probe, and the enabled runtime must answer the
# dummy argument with "invalid arg".
probe_rejects_arg() {
  probe "$1"
  if [[ "$CAPTURED_OUTPUT" != *"invalid arg"* ]]; then
    printf 'Unexpected hyprtasking:%s probe response: %s\n' "$1" "$CAPTURED_OUTPUT" >&2
    exit 1
  fi
}

assert_dispatchers_ready() {
  local name
  for name in toggle move select commit health; do
    case "$name" in
      toggle | move) probe_rejects_arg "$name" ;;
      *) probe "$name" ;;
    esac
  done

  # hyprctl dispatch prints "ok"; Lua eval prints the one-line summary.
  if [[ "$CAPTURED_OUTPUT" == *"runtime_enabled=false"* ]]; then
    printf 'Hyprtasking runtime is disabled: %s\n' "$CAPTURED_OUTPUT" >&2
    exit 1
  fi

  if [[ "$CAPTURED_OUTPUT" != "ok" && "$CAPTURED_OUTPUT" != *"runtime_enabled=true"* ]]; then
    printf 'Unexpected hyprtasking:health probe response: %s\n' "$CAPTURED_OUTPUT" >&2
    exit 1
  fi
}

smoke_dispatchers() {
  assert_plugin_loaded
  assert_dispatchers_ready
}

# Sets plugin:hyprtasking:<key>. Lua config managers reject `keyword`, so
# build the nested hl.config table instead (grid:rows -> grid = { rows = v }).
set_plugin_option() {
  local key=$1
  local value=$2

  if ! hyprland_uses_lua_plugin_calls; then
    run_hyprctl keyword "plugin:hyprtasking:${key}" "$value"
    return
  fi

  local table="$value"
  local part
  local -a parts
  local i
  IFS=: read -r -a parts <<< "$key"
  for ((i = ${#parts[@]} - 1; i >= 0; i--)); do
    part=${parts[i]}
    table="{ ${part} = ${table} }"
  done
  run_hyprctl eval "hl.config({ plugin = { hyprtasking = ${table} } })"
}

restore_safe_options() {
  set_plugin_option drag_button 272
  set_plugin_option select_button 273
  set_plugin_option gestures:move_fingers 3
  set_plugin_option gestures:open_fingers 4
  set_plugin_option grid:rows 3
  set_plugin_option grid:cols 3
  set_plugin_option gestures:move_distance 300
  set_plugin_option gestures:open_distance 300
  run_capture_hyprctl plugin unload "$PLUGIN_PATH"
  run_capture_hyprctl plugin load "$PLUGIN_PATH"
  wait_for_plugin_ready
  smoke_dispatchers
}

# Invalid values are rejected and the previous config kept; the runtime must
# stay enabled, so toggle still answers the probe with "invalid arg". The
# error itself goes to the log and a notification.
smoke_runtime_safety() {
  assert_plugin_loaded
  restore_safe_options

  set_plugin_option "grid:rows" 0
  probe_rejects_arg toggle
  restore_safe_options

  set_plugin_option "grid:cols" 0
  probe_rejects_arg toggle
  restore_safe_options

  set_plugin_option "gestures:move_distance" 0
  probe_rejects_arg toggle
  restore_safe_options

  set_plugin_option "gestures:open_distance" 0
  probe_rejects_arg toggle
  restore_safe_options

  set_plugin_option "drag_button" 272
  set_plugin_option "select_button" 272
  probe_rejects_arg toggle
  restore_safe_options

  set_plugin_option "gestures:move_fingers" 3
  set_plugin_option "gestures:open_fingers" 3
  probe_rejects_arg toggle
  restore_safe_options
}

wait_for_plugin_ready() {
  local attempts=40
  local delay=0.1
  local i
  local saw_plugin=0

  for ((i = 0; i < attempts; i++)); do
    if plugin_listed; then
      saw_plugin=1
      run_hyprtasking hyprtasking:toggle __smoke_probe__
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

smoke_load_unload() {
  if [[ ! -f "$PLUGIN_PATH" ]]; then
    printf 'Plugin path does not exist: %s\n' "$PLUGIN_PATH" >&2
    exit 1
  fi

  local cycle
  for ((cycle = 0; cycle < LOAD_UNLOAD_CYCLES; cycle++)); do
    run_capture_hyprctl plugin unload "$PLUGIN_PATH"
    if [[ "$CAPTURED_OUTPUT" == *"plugin not loaded"* ]] && plugin_listed; then
      printf 'Target plugin path is not the currently loaded Hyprtasking instance: %s\n' "$PLUGIN_PATH" >&2
      printf 'A different Hyprtasking instance is already loaded.\n' >&2
      printf 'Current plugin list:\n%s\n' "$CAPTURED_OUTPUT" >&2
      exit 1
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

case "$MODE" in
  all | stress)
    smoke_load_unload
    smoke_dispatchers
    smoke_runtime_safety
    smoke_toggle
    smoke_reload
    smoke_reload_open
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
  toggle)
    smoke_toggle
    ;;
  reload)
    smoke_reload
    ;;
  reload-open)
    smoke_reload_open
    ;;
  *)
    printf 'Usage: %s [all|stress|dispatchers|safety|load-unload|toggle|reload|reload-open]\n' "$0" >&2
    exit 1
    ;;
esac
