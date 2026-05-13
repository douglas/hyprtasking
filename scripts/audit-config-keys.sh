#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
AUDIT_CONFIG_KEYS_FORMAT=${AUDIT_CONFIG_KEYS_FORMAT:-text}

failures=0
missing_keys=()

print_out() {
  if [[ "$AUDIT_CONFIG_KEYS_FORMAT" != "json" ]]; then
    printf '%b' "$1"
  fi
}

print_err() {
  if [[ "$AUDIT_CONFIG_KEYS_FORMAT" != "json" ]]; then
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
  local i

  if [[ "$AUDIT_CONFIG_KEYS_FORMAT" != "json" ]]; then
    return
  fi

  printf '{'
  printf '"status":%s,' "$(json_string "$status")"
  printf '"repo":%s,' "$(json_string "$REPO_DIR")"
  printf '"failure_count":%s,' "$failures"
  printf '"missing_keys":['
  for ((i = 0; i < ${#missing_keys[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '%s' "$(json_string "${missing_keys[i]}")"
  done
  printf ']'
  printf '}\n'
}

collect_used_keys() {
  rg -o --pcre2 '(?:(?:HTConfig::)?value(?:_or)?<[^>]+>|(?:int|float)_config_or)\("([^"]+)"' "$REPO_DIR/src" --glob '!src/config.hpp' \
    | sed -E 's/.*\("([^"]+)"/\1/' \
    | sort -u
}

collect_registered_keys() {
  {
    rg -o 'plugin:hyprtasking:[^"]+' "$REPO_DIR/src" \
      | sed -E 's/^plugin:hyprtasking://' || true
    rg -o --pcre2 'add_(?:int|float)_config\("([^"]+)"' "$REPO_DIR/src/config.cpp" \
      | sed -E 's/.*\("([^"]+)"/\1/'
  } | sort -u
}

print_out 'Hyprtasking config key audit\n'
print_out "Repo: $REPO_DIR\n"
print_out 'Comparing HTConfig key usage vs initializeConfig registrations.\n'

mapfile -t used_keys < <(collect_used_keys)
mapfile -t registered_keys < <(collect_registered_keys)

if [[ ${#used_keys[@]} -eq 0 ]]; then
  print_err 'No config keys detected from HTConfig usage.\n'
  failures=$((failures + 1))
fi

registered_blob=$'\n'"$(printf '%s\n' "${registered_keys[@]}")"$'\n'
for key in "${used_keys[@]}"; do
  [[ -z "$key" ]] && continue
  if [[ "$registered_blob" != *$'\n'"$key"$'\n'* ]]; then
    missing_keys+=("$key")
    failures=$((failures + 1))
  fi
done

if ((failures > 0)); then
  print_err 'forbidden: missing config registrations for used keys\n'
  for key in "${missing_keys[@]}"; do
    print_err "  - $key\n"
  done
  print_err '\nConfig key audit failed.\n'
  emit_json_result "failed"
  exit 1
fi

print_out 'ok: all used keys are registered in initializeConfig\n'
emit_json_result "ok"
