#!/usr/bin/env bash
# Checks a Hyprland source tree against scripts/compat-contracts.tsv.
# Exit codes: 2 missing file, 3 unsupported version, 4 contract drift.
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$SCRIPT_DIR/support-matrix.sh"

HYPRLAND_SOURCE=${1:-${HYPRLAND_SOURCE:-}}
CONTRACTS_FILE="$SCRIPT_DIR/compat-contracts.tsv"

if [[ -z "$HYPRLAND_SOURCE" ]]; then
  printf 'Usage: %s <hyprland-source-path>\n' "${0##*/}" >&2
  exit 1
fi

# Literal pattern -> regex that tolerates whitespace changes.
loose_regex() {
  printf '%s' "$1" | sed -E 's/[][(){}.^$+*?|\\]/\\&/g; s/[[:space:]]+/[[:space:]]+/g'
}

# True if any @@@ pattern alternative matches in any @@@ path alternative.
pattern_matches() {
  local patterns=$1 paths=$2 path pattern
  while IFS= read -r path; do
    [[ -f "$HYPRLAND_SOURCE/$path" ]] || continue
    while IFS= read -r pattern; do
      [[ -n "$pattern" ]] || continue
      if rg -q -U "$(loose_regex "$pattern")" "$HYPRLAND_SOURCE/$path"; then
        return 0
      fi
    done <<< "${patterns//@@@/$'\n'}"
  done <<< "${paths//@@@/$'\n'}"
  return 1
}

any_file_exists() {
  local path
  while IFS= read -r path; do
    [[ -n "$path" && -f "$HYPRLAND_SOURCE/$path" ]] && return 0
  done <<< "${1//@@@/$'\n'}"
  return 1
}

if [[ ! -f "$HYPRLAND_SOURCE/VERSION" ]]; then
  printf 'Missing required file: %s/VERSION\n' "$HYPRLAND_SOURCE" >&2
  exit 2
fi
TARGET_VERSION="$(tr -d '[:space:]' < "$HYPRLAND_SOURCE/VERSION")"

printf 'Hyprtasking compat audit\n'
printf 'Source: %s (Hyprland %s)\n' "$HYPRLAND_SOURCE" "$TARGET_VERSION"
printf 'Supported targets: %s\n' "$SUPPORTED_TARGETS"

if ! supported_target "$TARGET_VERSION" >/dev/null; then
  printf 'Unsupported Hyprland version for this plugin line: %s\n' "$TARGET_VERSION" >&2
  exit 3
fi

failures=0
while IFS=$'\t' read -r label paths owner patterns; do
  [[ -z "$label" || "$label" == \#* ]] && continue

  if ! any_file_exists "$paths"; then
    printf 'Missing required file: %s/%s\n' "$HYPRLAND_SOURCE" "$paths" >&2
    exit 2
  fi

  missing=()
  while IFS= read -r pattern; do
    [[ -n "$pattern" ]] || continue
    pattern_matches "$pattern" "$paths" || missing+=("$pattern")
  done <<< "${patterns//|||/$'\n'}"

  if ((${#missing[@]} == 0)); then
    printf 'ok: %s\n' "$label"
    continue
  fi

  failures=$((failures + 1))
  printf 'drift: %s (%s)\n' "$label" "$paths" >&2
  printf '  touchpoint: %s\n' "$owner" >&2
  printf '  missing: %s\n' "${missing[@]}" >&2
done < "$CONTRACTS_FILE"

if ((failures > 0)); then
  printf '\nCompat audit failed: %d contract(s) drifted. See docs/compat-contract.md.\n' \
    "$failures" >&2
  exit 4
fi

printf '\nCompat audit passed for Hyprland %s.\n' "$TARGET_VERSION"
