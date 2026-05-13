#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
source "$SCRIPT_DIR/compat-contract-manifest.sh"

AUDIT_COVERAGE_FORMAT=${AUDIT_COVERAGE_FORMAT:-text}
EXIT_UNMAPPED_SYMBOLS=4
TOKEN_REGEX='g_p[A-Za-z0-9_]+|\bm_[A-Za-z0-9_]+\b|Event::bus|changeWorkspace|dragController|removeAllOfType|onMouseButton|findFunctionsByName|removeFunctionHook|logicalBox|getWindowMainSurfaceBox|workspaceID|monitorID'

print_out() {
  if [[ "$AUDIT_COVERAGE_FORMAT" != "json" ]]; then
    printf '%b' "$1"
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
  local checked_count=$4
  local mapped_count=$5
  local i=0

  if [[ "$AUDIT_COVERAGE_FORMAT" != "json" ]]; then
    return
  fi

  printf '{'
  printf '"status":%s,' "$(json_string "$status")"
  printf '"exit_code":%s,' "$exit_code"
  printf '"message":%s,' "$(json_string "$message")"
  printf '"compat_dir":%s,' "$(json_string "$REPO_DIR/src/compat")"
  printf '"checked_symbol_count":%s,' "$checked_count"
  printf '"mapped_symbol_count":%s,' "$mapped_count"
  printf '"unmapped_symbols":['
  for ((i = 0; i < ${#UNMAPPED_SYMBOLS[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '%s' "$(json_string "${UNMAPPED_SYMBOLS[i]}")"
  done
  printf ']'
  printf '}\n'
}

read_symbols() {
  local input=$1
  if [[ -z "$input" ]]; then
    return
  fi
  printf '%s\n' "$input" | sed '/^$/d' | sort -u
}

SOURCE_SYMBOLS="$(
  rg -o --no-filename -N -- "$TOKEN_REGEX" "$REPO_DIR/src/compat" \
    | sed '/^$/d' \
    | sort -u
)"

MANIFEST_SYMBOLS="$(
  {
    compat_core_contracts_stream
    compat_surface_contracts_stream
  } \
    | cut -f4 \
    | tr '|' '\n' \
    | rg -o --no-filename -N -- "$TOKEN_REGEX" \
    | sed '/^$/d' \
    | sort -u
)"

UNMAPPED_RAW="$(
  comm -23 \
    <(read_symbols "$SOURCE_SYMBOLS") \
    <(read_symbols "$MANIFEST_SYMBOLS")
)"

mapfile -t UNMAPPED_SYMBOLS <<< "$UNMAPPED_RAW"
if [[ ${#UNMAPPED_SYMBOLS[@]} -eq 1 && -z "${UNMAPPED_SYMBOLS[0]}" ]]; then
  UNMAPPED_SYMBOLS=()
fi
source_count=$(read_symbols "$SOURCE_SYMBOLS" | wc -l)
mapped_count=$(read_symbols "$MANIFEST_SYMBOLS" | wc -l)

print_out "Hyprtasking compat coverage audit\n"
print_out "Compat directory: $REPO_DIR/src/compat\n"
print_out "Checked symbols: $source_count\n"

if [[ ${#UNMAPPED_SYMBOLS[@]} -gt 0 && -n "${UNMAPPED_SYMBOLS[0]}" ]]; then
  print_out "Unmapped compat-sensitive symbols:\n"
  for symbol in "${UNMAPPED_SYMBOLS[@]}"; do
    [[ -z "$symbol" ]] && continue
    print_out " - $symbol\n"
  done
  emit_json_result \
    "unmapped_symbols" \
    "$EXIT_UNMAPPED_SYMBOLS" \
    "One or more compat-sensitive symbols are not represented in the contract manifest." \
    "$source_count" \
    "$mapped_count"
  exit "$EXIT_UNMAPPED_SYMBOLS"
fi

print_out "All compat-sensitive symbols are mapped in the contract manifest.\n"
emit_json_result "ok" 0 "Compat coverage audit passed." "$source_count" "$mapped_count"
