#!/usr/bin/env bash
set -euo pipefail

run() {
  printf '$ %s\n' "$*"
  "$@"
}

if ! command -v hyprctl >/dev/null 2>&1; then
  printf 'hyprctl is required for the live smoke test.\n' >&2
  exit 1
fi

run hyprctl plugin list
run hyprctl dispatch hyprtasking:toggle cursor
run hyprctl dispatch hyprtasking:move right
run hyprctl dispatch hyprtasking:move left
run hyprctl dispatch hyprtasking:toggle cursor
run hyprctl plugin list

printf '\nManual follow-up:\n'
printf '1. Drag a tiled window across workspaces in the overview.\n'
printf '2. Move a hovered window with hyprtasking:movewindow.\n'
printf '3. Trigger a gesture open/close sequence.\n'
