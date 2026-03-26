#!/usr/bin/env bash
set -euo pipefail

plugin_path="${1:-$(realpath build/libhyprtasking.so)}"

run() {
  printf '$ %s\n' "$*"
  "$@"
}

if ! command -v hyprctl >/dev/null 2>&1; then
  printf 'hyprctl is required for the live smoke test.\n' >&2
  exit 1
fi

if [[ ! -f "$plugin_path" ]]; then
  printf 'Plugin not found at %s\n' "$plugin_path" >&2
  exit 1
fi

run hyprctl plugin list
run hyprctl dispatch hyprtasking:toggle cursor
run hyprctl dispatch hyprtasking:move right
run hyprctl dispatch hyprtasking:move left
run hyprctl dispatch hyprtasking:setoffset +1
run hyprctl dispatch hyprtasking:setoffset 0
run hyprctl dispatch hyprtasking:toggle cursor
run hyprctl plugin list

printf '\nManual follow-up:\n'
printf '1. Drag a tiled window across workspaces in the overview.\n'
printf '2. Trigger a gesture open/close sequence.\n'
printf '3. Reload Hyprland config once and repeat toggle + drag.\n'
