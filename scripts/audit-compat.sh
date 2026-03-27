#!/usr/bin/env bash
set -euo pipefail

HYPRLAND_SOURCE=${1:-${HYPRLAND_SOURCE:-/home/douglas/src/Hyprland}}
SUPPORTED_MINOR="0.54.x"
SUPPORTED_PREFIX="0.54."

require_file() {
  local path=$1
  if [[ ! -f "$path" ]]; then
    printf 'Missing required file: %s\n' "$path" >&2
    exit 1
  fi
}

require_match() {
  local pattern=$1
  local path=$2
  local label=$3

  if ! rg -q --fixed-strings "$pattern" "$path"; then
    printf 'Missing %s in %s\n' "$label" "$path" >&2
    return 1
  fi

  printf 'ok: %s\n' "$label"
}

PLUGIN_API_HPP="$HYPRLAND_SOURCE/src/plugins/PluginAPI.hpp"
PLUGIN_API_CPP="$HYPRLAND_SOURCE/src/plugins/PluginAPI.cpp"
PLUGIN_SYSTEM_CPP="$HYPRLAND_SOURCE/src/plugins/PluginSystem.cpp"
RENDERER_CPP="$HYPRLAND_SOURCE/src/render/Renderer.cpp"
MONITOR_HPP="$HYPRLAND_SOURCE/src/helpers/Monitor.hpp"
HYPRLAND_PC="$HYPRLAND_SOURCE/hyprland.pc.in"
VERSION_FILE="$HYPRLAND_SOURCE/VERSION"
CHECK_LABELS=(
  "public version API"
  "version API implementation"
  "renderWorkspace symbol"
  "shouldRenderWindow symbol"
  "renderWindow symbol"
  "isSolitaryBlocked symbol"
  "plugin API version check"
)
FAILED_LABELS=()
FAILED_PATHS=()

require_file "$PLUGIN_API_HPP"
require_file "$PLUGIN_API_CPP"
require_file "$PLUGIN_SYSTEM_CPP"
require_file "$RENDERER_CPP"
require_file "$MONITOR_HPP"
require_file "$VERSION_FILE"

TARGET_VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"

printf 'Hyprtasking compat audit\n'
printf 'Source: %s\n' "$HYPRLAND_SOURCE"
printf 'Supported Hyprland line: %s\n' "$SUPPORTED_MINOR"
printf 'Detected target version: %s\n' "$TARGET_VERSION"

if [[ "$TARGET_VERSION" != ${SUPPORTED_PREFIX}* ]]; then
  printf 'Unsupported Hyprland version for this plugin line: %s\n' "$TARGET_VERSION" >&2
  printf 'Expected supported line: %s\n' "$SUPPORTED_MINOR" >&2
  printf 'Update the plugin compat layer before attempting to load against this tree.\n' >&2
  exit 1
fi

if [[ -f "$HYPRLAND_PC" ]]; then
  printf 'pkg-config version line:\n'
  rg -n '^Version:' "$HYPRLAND_PC" || true
fi

failures=0

record_failure() {
  local label=$1
  local path=$2
  FAILED_LABELS+=("$label")
  FAILED_PATHS+=("$path")
  failures=$((failures + 1))
}

require_match 'APICALL SVersionInfo getHyprlandVersion(HANDLE handle);' "$PLUGIN_API_HPP" 'public version API' || record_failure 'public version API' "$PLUGIN_API_HPP"
require_match 'APICALL SVersionInfo HyprlandAPI::getHyprlandVersion(HANDLE handle)' "$PLUGIN_API_CPP" 'version API implementation' || record_failure 'version API implementation' "$PLUGIN_API_CPP"
require_match 'renderWorkspace' "$RENDERER_CPP" 'renderWorkspace symbol' || record_failure 'renderWorkspace symbol' "$RENDERER_CPP"
require_match 'shouldRenderWindow' "$RENDERER_CPP" 'shouldRenderWindow symbol' || record_failure 'shouldRenderWindow symbol' "$RENDERER_CPP"
require_match 'renderWindow' "$RENDERER_CPP" 'renderWindow symbol' || record_failure 'renderWindow symbol' "$RENDERER_CPP"
require_match 'isSolitaryBlocked' "$MONITOR_HPP" 'isSolitaryBlocked symbol' || record_failure 'isSolitaryBlocked symbol' "$MONITOR_HPP"
require_match 'HYPRLAND_API_VERSION' "$PLUGIN_SYSTEM_CPP" 'plugin API version check' || record_failure 'plugin API version check' "$PLUGIN_SYSTEM_CPP"

printf '\nChecked contracts (%d):\n' "${#CHECK_LABELS[@]}"
printf ' - %s\n' "${CHECK_LABELS[@]}"

if ((failures > 0)); then
  printf '\nCompat audit failed with %d issue(s).\n' "$failures" >&2
  printf 'Missing or changed contracts:\n' >&2
  i=0
  for ((i = 0; i < ${#FAILED_LABELS[@]}; i++)); do
    printf ' - %s (%s)\n' "${FAILED_LABELS[i]}" "${FAILED_PATHS[i]}" >&2
  done
  printf 'Expected fix location: src/compat/ in this plugin tree.\n' >&2
  printf 'Rerun with HYPRLAND_SOURCE set to the target Hyprland checkout after patching.\n' >&2
  exit 1
fi

printf '\nCompat audit passed for Hyprland %s on supported line %s.\n' "$TARGET_VERSION" "$SUPPORTED_MINOR"
