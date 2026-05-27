#!/usr/bin/env bash
set -euo pipefail

HYPRLAND_SOURCE=${1:-${HYPRLAND_SOURCE:-/home/douglas/src/hyprland}}
AUDIT_FORMAT=${AUDIT_FORMAT:-text}
EXIT_MISSING_FILE=2
EXIT_UNSUPPORTED_VERSION=3
EXIT_CONTRACT_DRIFT=4
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$SCRIPT_DIR/support-matrix.sh"
source "$SCRIPT_DIR/compat-contract-manifest.sh"

CHECK_LABELS=()
FAILED_LABELS=()
FAILED_PATHS=()
FAILED_OWNERS=()

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

version_supported() {
  local version candidate
  version=$1
  for candidate in "${SUPPORTED_VERSIONS[@]}"; do
    if [[ "$version" == "$candidate" || "$version" == "$candidate".* ]]; then
      return 0
    fi
  done
  return 1
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

pattern_to_loose_regex() {
  local literal=$1
  local escaped
  escaped="$(printf '%s' "$literal" | sed -E 's/[][(){}.^$+*?|\\]/\\&/g')"
  escaped="$(printf '%s' "$escaped" | sed -E 's/[[:space:]]+/[[:space:]]+/g')"
  printf '%s' "$escaped"
}

contract_pattern_matches() {
  local literal=$1
  local path=$2
  local regex
  regex="$(pattern_to_loose_regex "$literal")"
  rg -q -U "$regex" "$path"
}

contract_pattern_matches_any() {
  local literal=$1
  local relpath=$2
  local pattern_options path_options pattern path
  pattern_options="${literal//@@@/$'\n'}"
  path_options="${relpath//@@@/$'\n'}"

  while IFS= read -r path; do
    [[ -z "$path" ]] && continue
    path="$HYPRLAND_SOURCE/$path"
    [[ -f "$path" ]] || continue

    while IFS= read -r pattern; do
      [[ -z "$pattern" ]] && continue
      if contract_pattern_matches "$pattern" "$path"; then
        return 0
      fi
    done <<< "$pattern_options"
  done <<< "$path_options"

  return 1
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
  printf '"supported_targets":%s,' "$(json_string "$SUPPORTED_TARGETS")"
  printf '"supported_versions":%s,' "$(json_string "$SUPPORTED_VERSIONS_TEXT")"
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

require_any_file() {
  local relpath=$1
  local path_options path
  path_options="${relpath//@@@/$'\n'}"

  while IFS= read -r path; do
    [[ -z "$path" ]] && continue
    if [[ -f "$HYPRLAND_SOURCE/$path" ]]; then
      return
    fi
  done <<< "$path_options"

  print_err "Missing required file: $HYPRLAND_SOURCE/$relpath\n"
  emit_json_result "missing_file" "$EXIT_MISSING_FILE" "Missing required file: $HYPRLAND_SOURCE/$relpath"
  exit "$EXIT_MISSING_FILE"
}

record_failure() {
  local label=$1
  local path=$2
  local owner=$3
  FAILED_LABELS+=("$label")
  FAILED_PATHS+=("$path")
  FAILED_OWNERS+=("$owner")
}

check_contract_group() {
  local relpath=$1
  local label=$2
  local owner=$3
  shift 3

  CHECK_LABELS+=("$label")

  local missing=0
  local pattern
  for pattern in "$@"; do
    if ! contract_pattern_matches_any "$pattern" "$relpath"; then
      missing=1
      print_err "Missing $label pattern in $HYPRLAND_SOURCE/$relpath: $pattern\n"
    fi
  done

  if ((missing == 0)); then
    print_out "ok: $label\n"
    return
  fi

  record_failure "$label" "$HYPRLAND_SOURCE/$relpath" "$owner"
}

VERSION_FILE="$HYPRLAND_SOURCE/VERSION"
require_file "$VERSION_FILE"

TARGET_VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"

print_out "Hyprtasking compat surface audit\n"
print_out "Source: $HYPRLAND_SOURCE\n"
print_out "Supported Hyprland targets: $SUPPORTED_TARGETS\n"
print_out "Supported versions: $SUPPORTED_VERSIONS_TEXT\n"
print_out "Detected target version: $TARGET_VERSION\n"

if ! version_supported "$TARGET_VERSION"; then
  print_err "Unsupported Hyprland version for this plugin line: $TARGET_VERSION\n"
  print_err "Expected supported targets: $SUPPORTED_TARGETS\n"
  print_err "Supported versions: $SUPPORTED_VERSIONS_TEXT\n"
  print_err "Update the plugin compat layer before attempting to load against this tree.\n"
  emit_json_result "unsupported_version" "$EXIT_UNSUPPORTED_VERSION" "Unsupported Hyprland version for this plugin line: $TARGET_VERSION"
  exit "$EXIT_UNSUPPORTED_VERSION"
fi

while IFS=$'\t' read -r label relpath owner patterns; do
  [[ -z "$label" ]] && continue

  require_any_file "$relpath"

  pattern_blob="${patterns//|||/$'\n'}"
  mapfile -t pattern_lines <<< "$pattern_blob"
  check_contract_group "$relpath" "$label" "$owner" "${pattern_lines[@]}"
done < <(compat_surface_contracts_stream)

print_out "\nChecked compat surface groups (${#CHECK_LABELS[@]}):\n"
for label in "${CHECK_LABELS[@]}"; do
  print_out " - $label\n"
done

if (( ${#FAILED_LABELS[@]} > 0 )); then
  print_err "\nCompat surface audit failed with ${#FAILED_LABELS[@]} issue(s).\n"
  print_err "Missing or changed runtime-sensitive contracts:\n"
  for ((i = 0; i < ${#FAILED_LABELS[@]}; i++)); do
    print_err " - ${FAILED_LABELS[i]} (${FAILED_PATHS[i]})\n"
    print_err "   Suggested plugin touchpoint: ${FAILED_OWNERS[i]}\n"
  done
  print_err "Compat contract reference: docs/compat-contract.md\n"
  print_err "Rerun with HYPRLAND_SOURCE set to the target Hyprland checkout after patching.\n"
  emit_json_result "contract_drift" "$EXIT_CONTRACT_DRIFT" "One or more audited Hyprland runtime compat contracts drifted on a supported target."
  exit "$EXIT_CONTRACT_DRIFT"
fi

print_out "\nCompat surface audit passed for Hyprland $TARGET_VERSION on supported targets $SUPPORTED_TARGETS.\n"
emit_json_result "ok" 0 "Compat surface audit passed."
