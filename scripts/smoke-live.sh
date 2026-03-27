#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PLUGIN_PATH=${PLUGIN_PATH:-"$SCRIPT_DIR/../build/libhyprtasking.so"}
PLUGIN_PATH=$(realpath "$PLUGIN_PATH")
MODE=${1:-all}
LOAD_UNLOAD_CYCLES=${LOAD_UNLOAD_CYCLES:-1}
TOGGLE_CYCLES=${TOGGLE_CYCLES:-1}
RELOAD_CYCLES=${RELOAD_CYCLES:-1}
PRINT_MANUAL_FOLLOW_UP=${PRINT_MANUAL_FOLLOW_UP:-1}
HYPRCTL_PREFIX=()

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
  local runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

  if [[ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]]; then
    if [[ -S "${runtime_dir}/hypr/${HYPRLAND_INSTANCE_SIGNATURE}/.socket.sock" ]]; then
      HYPRCTL_PREFIX=(env "HYPRLAND_INSTANCE_SIGNATURE=${HYPRLAND_INSTANCE_SIGNATURE}" hyprctl)
      return
    fi
  fi

  local detected_signature
  detected_signature="$(hyprctl instances 2>/dev/null | sed -n 's/^instance \(.*\):$/\1/p' | head -n 1)"
  if [[ -n "$detected_signature" ]]; then
    HYPRCTL_PREFIX=(env "HYPRLAND_INSTANCE_SIGNATURE=${detected_signature}" hyprctl)
    return
  fi

  HYPRCTL_PREFIX=(hyprctl)
}

run_hyprctl() {
  run "${HYPRCTL_PREFIX[@]}" "$@"
}

run_capture_hyprctl() {
  run_capture "${HYPRCTL_PREFIX[@]}" "$@"
}

assert_plugin_loaded() {
  run_capture_hyprctl plugin list
  if ! printf '%s\n' "$CAPTURED_OUTPUT" | rg -q '^Plugin Hyprtasking by '; then
    printf 'Hyprtasking is not loaded.\n' >&2
    exit 1
  fi
}

assert_dispatcher_ready() {
  run_capture_hyprctl dispatch hyprtasking:toggle __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    printf 'Hyprtasking dispatchers are not registered yet.\n' >&2
    exit 1
  fi
}

assert_extended_dispatchers_ready() {
  assert_dispatcher_ready

  run_capture_hyprctl dispatch hyprtasking:move __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    printf 'Hyprtasking move dispatcher is not registered yet.\n' >&2
    exit 1
  fi

  run_capture_hyprctl dispatch hyprtasking:movewindow __smoke_probe__
  if [[ "$CAPTURED_OUTPUT" == *"Invalid dispatcher"* ]]; then
    printf 'Hyprtasking movewindow dispatcher is not registered yet.\n' >&2
    exit 1
  fi
}

wait_for_plugin_ready() {
  local attempts=${1:-40}
  local delay=${2:-0.1}
  local i

  for ((i = 0; i < attempts; i++)); do
    run_capture_hyprctl plugin list
    if printf '%s\n' "$CAPTURED_OUTPUT" | rg -q '^Plugin Hyprtasking by '; then
      run_capture_hyprctl dispatch hyprtasking:toggle __smoke_probe__
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

init_hyprctl_prefix

smoke_toggle() {
  local cycle
  for ((cycle = 0; cycle < TOGGLE_CYCLES; cycle++)); do
    assert_plugin_loaded
    assert_extended_dispatchers_ready
    run_hyprctl dispatch hyprtasking:toggle cursor
    run_hyprctl dispatch hyprtasking:move right
    run_hyprctl dispatch hyprtasking:move left
    run_hyprctl dispatch hyprtasking:toggle cursor
  done
  run_hyprctl plugin list
}

smoke_reload() {
  local cycle
  for ((cycle = 0; cycle < RELOAD_CYCLES; cycle++)); do
    assert_plugin_loaded
    run_hyprctl reload
    wait_for_plugin_ready
    assert_plugin_loaded
    assert_extended_dispatchers_ready
  done
}

smoke_reload_open() {
  local cycle
  for ((cycle = 0; cycle < RELOAD_CYCLES; cycle++)); do
    assert_plugin_loaded
    assert_extended_dispatchers_ready
    run_hyprctl dispatch hyprtasking:toggle cursor
    run_hyprctl reload
    wait_for_plugin_ready
    assert_plugin_loaded
    assert_extended_dispatchers_ready
    run_hyprctl dispatch hyprtasking:toggle cursor
    run_hyprctl dispatch hyprtasking:toggle cursor
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
    run_hyprctl plugin unload "$PLUGIN_PATH"
    run_hyprctl plugin load "$PLUGIN_PATH"
    wait_for_plugin_ready
    assert_plugin_loaded
  done
}

run_manual_follow_up() {
  run bash "$SCRIPT_DIR/manual-runtime-check.sh"
}

print_manual_follow_up_if_enabled() {
  if [[ "$PRINT_MANUAL_FOLLOW_UP" == "1" ]]; then
    run_manual_follow_up
  fi
}

case "$MODE" in
  all)
    smoke_load_unload
    smoke_toggle
    smoke_reload
    smoke_reload_open
    print_manual_follow_up_if_enabled
    ;;
  stress)
    smoke_stress
    print_manual_follow_up_if_enabled
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
    run_manual_follow_up
    ;;
  *)
    printf 'Usage: %s [all|stress|load-unload|toggle|move|reload|reload-open|manual]\n' "$0" >&2
    exit 1
    ;;
esac
