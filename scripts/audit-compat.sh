#!/usr/bin/env bash
set -euo pipefail

HYPRLAND_SOURCE=${1:-${HYPRLAND_SOURCE:-/home/douglas/src/Hyprland}}
SUPPORTED_MINOR="0.54.x"
SUPPORTED_PREFIX="0.54."
AUDIT_FORMAT=${AUDIT_FORMAT:-text}
EXIT_MISSING_FILE=2
EXIT_UNSUPPORTED_VERSION=3
EXIT_CONTRACT_DRIFT=4

print_out() {
  if [[ "$AUDIT_FORMAT" != "json" ]]; then
    printf '%b' "$1"
  fi
}

print_err() {
  if [[ "$AUDIT_FORMAT" != "json" ]]; then
    printf '%b' "$1" >&2
  fi
}

json_escape() {
  local value=$1
  value=${value//\\/\\\\}
  value=${value//\"/\\\"}
  value=${value//$'\n'/\\n}
  value=${value//$'\r'/\\r}
  value=${value//$'\t'/\\t}
  printf '%s' "$value"
}

json_string() {
  printf '"%s"' "$(json_escape "$1")"
}

emit_json_result() {
  local status=$1
  local exit_code=$2
  local message=$3
  local i

  if [[ "$AUDIT_FORMAT" != "json" ]]; then
    return
  fi

  printf '{'
  printf '"status":%s,' "$(json_string "$status")"
  printf '"exit_code":%s,' "$exit_code"
  printf '"source":%s,' "$(json_string "$HYPRLAND_SOURCE")"
  printf '"supported_line":%s,' "$(json_string "$SUPPORTED_MINOR")"
  printf '"target_version":%s,' "$(json_string "${TARGET_VERSION:-}")"
  printf '"message":%s,' "$(json_string "$message")"

  printf '"checked_contracts":['
  for ((i = 0; i < ${#CHECK_LABELS[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '%s' "$(json_string "${CHECK_LABELS[i]}")"
  done
  printf '],'

  printf '"failed_contracts":['
  for ((i = 0; i < ${#FAILED_LABELS[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '{'
    printf '"label":%s,' "$(json_string "${FAILED_LABELS[i]}")"
    printf '"source_path":%s,' "$(json_string "${FAILED_PATHS[i]}")"
    printf '"suggested_touchpoint":%s' "$(json_string "${FAILED_OWNERS[i]}")"
    printf '}'
  done
  printf ']'
  printf '}\n'
}

require_file() {
  local path=$1
  if [[ ! -f "$path" ]]; then
    print_err "Missing required file: $path\n"
    emit_json_result "missing_file" "$EXIT_MISSING_FILE" "Missing required file: $path"
    exit "$EXIT_MISSING_FILE"
  fi
}

require_match() {
  local pattern=$1
  local path=$2
  local label=$3

  if ! rg -q --fixed-strings "$pattern" "$path"; then
    print_err "Missing $label in $path\n"
    return 1
  fi

  print_out "ok: $label\n"
}

check_contract() {
  local pattern=$1
  local path=$2
  local label=$3
  local owner=$4

  if ! require_match "$pattern" "$path" "$label"; then
    record_failure "$label" "$path" "$owner"
  fi
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
  "signature fallback API"
  "signature fallback implementation"
  "renderWorkspace symbol"
  "shouldRenderWindow symbol"
  "renderWindow symbol"
  "isSolitaryBlocked symbol"
  "plugin API version check"
)
FAILED_LABELS=()
FAILED_PATHS=()
FAILED_OWNERS=()

require_file "$PLUGIN_API_HPP"
require_file "$PLUGIN_API_CPP"
require_file "$PLUGIN_SYSTEM_CPP"
require_file "$RENDERER_CPP"
require_file "$MONITOR_HPP"
require_file "$VERSION_FILE"

TARGET_VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"

print_out "Hyprtasking compat audit\n"
print_out "Source: $HYPRLAND_SOURCE\n"
print_out "Supported Hyprland line: $SUPPORTED_MINOR\n"
print_out "Detected target version: $TARGET_VERSION\n"

if [[ "$TARGET_VERSION" != ${SUPPORTED_PREFIX}* ]]; then
  print_err "Unsupported Hyprland version for this plugin line: $TARGET_VERSION\n"
  print_err "Expected supported line: $SUPPORTED_MINOR\n"
  print_err "Update the plugin compat layer before attempting to load against this tree.\n"
  emit_json_result "unsupported_version" "$EXIT_UNSUPPORTED_VERSION" "Unsupported Hyprland version for this plugin line: $TARGET_VERSION"
  exit "$EXIT_UNSUPPORTED_VERSION"
fi

if [[ -f "$HYPRLAND_PC" ]]; then
  print_out "pkg-config version line:\n"
  if [[ "$AUDIT_FORMAT" != "json" ]]; then
    rg -n '^Version:' "$HYPRLAND_PC" || true
  fi
fi

failures=0

record_failure() {
  local label=$1
  local path=$2
  local owner=$3
  FAILED_LABELS+=("$label")
  FAILED_PATHS+=("$path")
  FAILED_OWNERS+=("$owner")
  failures=$((failures + 1))
}

check_contract 'APICALL SVersionInfo getHyprlandVersion(HANDLE handle);' "$PLUGIN_API_HPP" 'public version API' 'src/compat/profile.cpp'
check_contract 'APICALL SVersionInfo HyprlandAPI::getHyprlandVersion(HANDLE handle)' "$PLUGIN_API_CPP" 'version API implementation' 'src/compat/profile.cpp'
check_contract 'APICALL [[deprecated]] void* getFunctionAddressFromSignature(HANDLE handle, const std::string& sig);' "$PLUGIN_API_HPP" 'signature fallback API' 'src/compat/profile.cpp'
check_contract 'APICALL void* HyprlandAPI::getFunctionAddressFromSignature(HANDLE handle, const std::string& sig)' "$PLUGIN_API_CPP" 'signature fallback implementation' 'src/compat/profile.cpp'
check_contract 'renderWorkspace' "$RENDERER_CPP" 'renderWorkspace symbol' 'src/compat/profile.cpp, src/compat/renderer_compat.cpp'
check_contract 'shouldRenderWindow' "$RENDERER_CPP" 'shouldRenderWindow symbol' 'src/compat/profile.cpp, src/compat/renderer_compat.cpp'
check_contract 'renderWindow' "$RENDERER_CPP" 'renderWindow symbol' 'src/compat/profile.cpp, src/compat/renderer_compat.cpp'
check_contract 'isSolitaryBlocked' "$MONITOR_HPP" 'isSolitaryBlocked symbol' 'src/compat/profile.cpp'
check_contract 'HYPRLAND_API_VERSION' "$PLUGIN_SYSTEM_CPP" 'plugin API version check' 'src/compat/profile.cpp'

print_out "\nChecked contracts (${#CHECK_LABELS[@]}):\n"
for label in "${CHECK_LABELS[@]}"; do
  print_out " - $label\n"
done

if ((failures > 0)); then
  print_err "\nCompat audit failed with $failures issue(s).\n"
  print_err "Missing or changed contracts:\n"
  i=0
  for ((i = 0; i < ${#FAILED_LABELS[@]}; i++)); do
    print_err " - ${FAILED_LABELS[i]} (${FAILED_PATHS[i]})\n"
    print_err "   Suggested plugin touchpoint: ${FAILED_OWNERS[i]}\n"
  done
  print_err "Compat contract reference: docs/compat-contract.md\n"
  print_err "Rerun with HYPRLAND_SOURCE set to the target Hyprland checkout after patching.\n"
  emit_json_result "contract_drift" "$EXIT_CONTRACT_DRIFT" "One or more audited Hyprland contracts drifted on a supported line."
  exit "$EXIT_CONTRACT_DRIFT"
fi

print_out "\nCompat audit passed for Hyprland $TARGET_VERSION on supported line $SUPPORTED_MINOR.\n"
emit_json_result "ok" 0 "Compat audit passed."
