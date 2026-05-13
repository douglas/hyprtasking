#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  printf 'Usage: %s <hyprland-source-path>\n' "${0##*/}" >&2
  exit 1
fi

HYPRLAND_SOURCE=$1
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)

bash "$SCRIPT_DIR/generate-compat-contract.sh" "$HYPRLAND_SOURCE"
bash "$SCRIPT_DIR/audit-compat.sh" "$HYPRLAND_SOURCE"
bash "$SCRIPT_DIR/audit-compat-surface.sh" "$HYPRLAND_SOURCE"
bash "$SCRIPT_DIR/audit-compat-coverage.sh"
bash "$SCRIPT_DIR/audit-boundary.sh"
bash "$SCRIPT_DIR/audit-guidelines.sh"
bash "$SCRIPT_DIR/audit-config-keys.sh"

if [[ -d "$REPO_DIR/build" ]]; then
  meson test -C "$REPO_DIR/build" logic-tests --print-errorlogs
else
  printf 'build directory not found, skipping logic-tests run.\n'
fi

printf 'Supported Hyprland update workflow completed for source: %s\n' "$HYPRLAND_SOURCE"
